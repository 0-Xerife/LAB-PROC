#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <Adafruit_NeoPixel.h>

// Nome da rede Wi-Fi que o ESP32 vai criar
const char *ssid = "AminhaVidaEstaNasMaosDeDeus";

// Mude para 1 para testar o modo desafio (Semáforo no NeoPixel)
#define ENABLE_TRAFFIC_CHALLENGE 0

const int LDR_PIN = 4;
const int SOS_BUTTON_PIN = 2;

// Pino e configuração do LED RGB embutido (NeoPixel) no ESP32-C3
const int BUILTIN_RGB_PIN = 8;
const int NUM_RGB_LEDS = 1;

const int ADC_BITS = 12;
const int ADC_MAX_VALUE = 4095;
// Limiar ajustado: valores acima de 2000 serão considerados baixa luminosidade
const int LOW_LIGHT_THRESHOLD = 2000;

const unsigned long LDR_READ_INTERVAL_MS = 1000;
const unsigned long SOS_ACTIVE_MS = 3000;
const unsigned long DEBOUNCE_MS = 50;

NetworkServer server(80);
Adafruit_NeoPixel rgb(NUM_RGB_LEDS, BUILTIN_RGB_PIN, NEO_GRB + NEO_KHZ800);

volatile bool sosInterruptFlag = false;
volatile unsigned long lastSosInterruptMs = 0;

int ldrValue = 0;
bool lowLight = false;
bool sosActive = false;
unsigned long sosUntilMs = 0;
unsigned long lastLdrReadMs = 0;

bool pedestrianRequest = false;
unsigned long trafficStateStartMs = 0;

enum TrafficState
{
  TRAFFIC_GREEN,
  TRAFFIC_YELLOW_TO_RED,
  TRAFFIC_RED_FOR_PEDESTRIAN
};

TrafficState trafficState = TRAFFIC_GREEN;

// HTML da página
const char *html_page = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Exp5 - Monitoramento Inteligente</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 30px; background: #f6f7fb; color: #111; }
    .card { max-width: 620px; padding: 24px; background: white; border-radius: 14px; box-shadow: 0 2px 10px #0002; }
    h1 { margin-top: 0; }
    .value { font-size: 28px; font-weight: bold; }
    .ok { color: #0a7f38; }
    .warn { color: #b77900; }
    .sos { color: #c40024; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Exp5 - Monitoramento Inteligente</h1>
    <p>Valor ADC do LDR:</p>
    <div class="value" id="adc">--</div>
    <p>Percentual bruto de luz: <span id="percent">--</span>%</p>
    <p>Luminosidade: <span id="light">--</span></p>
    <p>Emergência SOS: <span id="sos">--</span></p>
    <p>Modo do código: <span id="mode">--</span></p>
  </div>

  <script>
    async function updateData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        document.getElementById('adc').textContent = data.adc;
        document.getElementById('percent').textContent = data.percent;
        document.getElementById('light').textContent = data.lowLight ? 'BAIXA' : 'NORMAL';
        document.getElementById('light').className = data.lowLight ? 'warn' : 'ok';
        document.getElementById('sos').textContent = data.sosActive ? 'ATIVO' : 'INATIVO';
        document.getElementById('sos').className = data.sosActive ? 'sos' : 'ok';
        document.getElementById('mode').textContent = data.mode;
      } catch(e) {
        console.error("Erro ao buscar dados", e);
      }
    }
    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

// Interrupção do Botão SOS com Debounce
void IRAM_ATTR onSosButtonInterrupt()
{
  unsigned long now = millis();
  if (now - lastSosInterruptMs >= DEBOUNCE_MS)
  {
    sosInterruptFlag = true;
    lastSosInterruptMs = now;
  }
}

// Controle centralizado do NeoPixel
void setBuiltInRgb(uint8_t r, uint8_t g, uint8_t b)
{
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

void builtInOff()
{
  setBuiltInRgb(0, 0, 0);
}

void updateLdrReading()
{
  unsigned long now = millis();
  if (now - lastLdrReadMs >= LDR_READ_INTERVAL_MS)
  {
    lastLdrReadMs = now;
    ldrValue = analogRead(LDR_PIN);

    // CORREÇÃO DA LÓGICA: No seu módulo, valores altos significam escuridão
    lowLight = ldrValue > LOW_LIGHT_THRESHOLD;

    Serial.print("ADC LDR = ");
    Serial.print(ldrValue);
    Serial.print(" | baixa luminosidade = ");
    Serial.println(lowLight ? "SIM" : "NAO");
  }
}

String makeDataJson()
{
  // CORREÇÃO DA PORCENTAGEM: Invertemos o mapeamento para fazer sentido visualmente
  int percent = map(ldrValue, 0, ADC_MAX_VALUE, 100, 0);
  percent = constrain(percent, 0, 100); // Evita valores negativos ou acima de 100%

  String json = "{";
  json += "\"adc\":" + String(ldrValue) + ",";
  json += "\"percent\":" + String(percent) + ",";
  json += "\"lowLight\":" + String(lowLight ? "true" : "false") + ",";
  json += "\"sosActive\":" + String(sosActive ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(ENABLE_TRAFFIC_CHALLENGE ? "semaforo" : "monitoramento") + "\"";
  json += "}";
  return json;
}

void consumeSosInterruptIfNeeded()
{
  bool event = false;

  noInterrupts();
  if (sosInterruptFlag)
  {
    sosInterruptFlag = false;
    event = true;
  }
  interrupts();

  if (!event)
  {
    return;
  }

#if ENABLE_TRAFFIC_CHALLENGE
  pedestrianRequest = true;
  Serial.println("Botao de pedestre detectado por interrupcao.");
#else
  sosActive = true;
  sosUntilMs = millis() + SOS_ACTIVE_MS;
  Serial.println("SOS detectado por interrupcao. NeoPixel vermelho por 3 segundos.");
#endif
}

void updateMonitoringLed()
{
  unsigned long now = millis();

  // Prioridade Máxima: Botão SOS (NeoPixel Vermelho por 3 segundos)
  if (sosActive)
  {
    if (now < sosUntilMs)
    {
      setBuiltInRgb(255, 0, 0);
      return;
    }
    sosActive = false;
  }

  // Prioridade Secundária: Baixa luminosidade (NeoPixel Pisca amarelo a cada 2 segundos)
  if (lowLight)
  {
    bool ledOn = ((now / 1000) % 2) == 0;
    if (ledOn)
    {
      setBuiltInRgb(255, 150, 0);
    }
    else
    {
      builtInOff();
    }
  }
  else
  {
    builtInOff();
  }
}

void updateTrafficChallenge()
{
  unsigned long now = millis();

  if (lowLight)
  {
    pedestrianRequest = false;
    trafficState = TRAFFIC_GREEN;
    bool yellowOn = ((now / 500) % 2) == 0;
    if (yellowOn)
    {
      setBuiltInRgb(255, 150, 0);
    }
    else
    {
      builtInOff();
    }
    return;
  }

  switch (trafficState)
  {
  case TRAFFIC_GREEN:
    setBuiltInRgb(0, 255, 0);
    if (pedestrianRequest)
    {
      pedestrianRequest = false;
      trafficState = TRAFFIC_YELLOW_TO_RED;
      trafficStateStartMs = now;
      Serial.println("Travessia solicitada: verde -> amarelo.");
    }
    break;

  case TRAFFIC_YELLOW_TO_RED:
    setBuiltInRgb(255, 150, 0);
    if (now - trafficStateStartMs >= 2000)
    {
      trafficState = TRAFFIC_RED_FOR_PEDESTRIAN;
      trafficStateStartMs = now;
      Serial.println("Semaforo vermelho para travessia.");
    }
    break;

  case TRAFFIC_RED_FOR_PEDESTRIAN:
    setBuiltInRgb(255, 0, 0);
    if (now - trafficStateStartMs >= 5000)
    {
      trafficState = TRAFFIC_GREEN;
      trafficStateStartMs = now;
      Serial.println("Fim da travessia: retorno ao verde.");
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  // Configuração do ADC
  analogReadResolution(ADC_BITS);

  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);

  // Inicializa o NeoPixel nativo da placa
  rgb.begin();
  rgb.clear();
  rgb.show();

  attachInterrupt(digitalPinToInterrupt(SOS_BUTTON_PIN), onSosButtonInterrupt, FALLING);

  // Configuração do Access Point (Criar a rede Wi-Fi)
  Serial.println("Configurando access point...");
  if (!WiFi.softAP(ssid))
  {
    Serial.println("Falha na criacao do Soft AP.");
    while (1)
      ;
  }

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.begin();
  Serial.println("Server started.");
  Serial.println("Conecte seu celular/PC na rede Wi-Fi 'ESP32_Monitoramento' e acesse o IP acima no navegador.");
}

void loop()
{
  // Tratamento manual do Servidor Web HTTP
  NetworkClient client = server.accept();

  if (client)
  {
    String currentLine = "";
    String header = "";

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        header += c;

        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            // Se a requisição for para o endpoint de dados JSON
            if (header.indexOf("GET /data") >= 0)
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:application/json");
              client.println("Connection: close");
              client.println();
              client.print(makeDataJson());
            }
            // Qualquer outra requisição, manda a página HTML com o front-end
            else
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              client.print(html_page);
            }
            client.println();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
    }
    client.stop();
  }

  // Funções contínuas: leitura do LDR e botões
  updateLdrReading();
  consumeSosInterruptIfNeeded();

#if ENABLE_TRAFFIC_CHALLENGE
  updateTrafficChallenge();
#else
  updateMonitoringLed();
#endif
}