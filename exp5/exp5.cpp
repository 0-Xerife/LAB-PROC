#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";

#define ENABLE_TRAFFIC_CHALLENGE 0

const int LDR_PIN = 4;
const int SOS_BUTTON_PIN = 2;
const int EXTERNAL_LED_PIN = 5;

const int BUILTIN_RGB_PIN = 8;
const int NUM_RGB_LEDS = 1;

const int TRAFFIC_RED_PIN = 6;
const int TRAFFIC_YELLOW_PIN = 7;
const int TRAFFIC_GREEN_PIN = 10;

const int ADC_BITS = 12;
const int ADC_MAX_VALUE = 4095;
const int LOW_LIGHT_THRESHOLD = 1800;

const unsigned long LDR_READ_INTERVAL_MS = 1000;
const unsigned long LOW_LIGHT_BLINK_MS = 2000;
const unsigned long NIGHT_TRAFFIC_BLINK_MS = 1000;
const unsigned long SOS_ACTIVE_MS = 3000;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long EXTERNAL_LED_PULSE_MS = 300;

WebServer server(80);
Adafruit_NeoPixel rgb(NUM_RGB_LEDS, BUILTIN_RGB_PIN, NEO_GRB + NEO_KHZ800);

volatile bool sosInterruptFlag = false;
volatile unsigned long lastSosInterruptMs = 0;

int ldrValue = 0;
bool lowLight = false;
bool sosActive = false;
unsigned long sosUntilMs = 0;
unsigned long lastLdrReadMs = 0;
unsigned long externalLedUntilMs = 0;

bool pedestrianRequest = false;
unsigned long trafficStateStartMs = 0;

enum TrafficState {
  TRAFFIC_GREEN,
  TRAFFIC_YELLOW_TO_RED,
  TRAFFIC_RED_FOR_PEDESTRIAN
};

TrafficState trafficState = TRAFFIC_GREEN;

void IRAM_ATTR onSosButtonInterrupt() {
  unsigned long now = millis();
  if (now - lastSosInterruptMs >= DEBOUNCE_MS) {
    sosInterruptFlag = true;
    lastSosInterruptMs = now;
  }
}

void setBuiltInRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

void builtInOff() {
  setBuiltInRgb(0, 0, 0);
}

void updateLdrReading() {
  unsigned long now = millis();
  if (now - lastLdrReadMs >= LDR_READ_INTERVAL_MS) {
    lastLdrReadMs = now;
    ldrValue = analogRead(LDR_PIN);
    lowLight = ldrValue < LOW_LIGHT_THRESHOLD;

    Serial.print("ADC LDR = ");
    Serial.print(ldrValue);
    Serial.print(" | baixa luminosidade = ");
    Serial.println(lowLight ? "SIM" : "NAO");
  }
}

String makeDataJson() {
  int percent = map(ldrValue, 0, ADC_MAX_VALUE, 0, 100);
  String json = "{";
  json += "\"adc\":" + String(ldrValue) + ",";
  json += "\"percent\":" + String(percent) + ",";
  json += "\"lowLight\":" + String(lowLight ? "true" : "false") + ",";
  json += "\"sosActive\":" + String(sosActive ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(ENABLE_TRAFFIC_CHALLENGE ? "semaforo" : "monitoramento") + "\"";
  json += "}";
  return json;
}

void handleRoot() {
  String html = R"rawliteral(
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
    <p>Percentual bruto: <span id="percent">--</span>%</p>
    <p>Luminosidade: <span id="light">--</span></p>
    <p>Emergência SOS: <span id="sos">--</span></p>
    <p>Modo do código: <span id="mode">--</span></p>
  </div>

  <script>
    async function updateData() {
      const response = await fetch('/data');
      const data = await response.json();
      document.getElementById('adc').textContent = data.adc;
      document.getElementById('percent').textContent = data.percent;
      document.getElementById('light').textContent = data.lowLight ? 'BAIXA' : 'NORMAL';
      document.getElementById('light').className = data.lowLight ? 'warn' : 'ok';
      document.getElementById('sos').textContent = data.sosActive ? 'ATIVO' : 'INATIVO';
      document.getElementById('sos').className = data.sosActive ? 'sos' : 'ok';
      document.getElementById('mode').textContent = data.mode;
    }
    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  server.send(200, "application/json", makeDataJson());
}

void consumeSosInterruptIfNeeded() {
  bool event = false;

  noInterrupts();
  if (sosInterruptFlag) {
    sosInterruptFlag = false;
    event = true;
  }
  interrupts();

  if (!event) {
    return;
  }

#if ENABLE_TRAFFIC_CHALLENGE
  pedestrianRequest = true;
  Serial.println("Botao de pedestre detectado por interrupcao.");
#else
  sosActive = true;
  sosUntilMs = millis() + SOS_ACTIVE_MS;
  externalLedUntilMs = millis() + EXTERNAL_LED_PULSE_MS;
  digitalWrite(EXTERNAL_LED_PIN, HIGH);
  Serial.println("SOS detectado por interrupcao. LED vermelho por 3 segundos.");
#endif
}

void updateExternalLedPulse() {
  if (externalLedUntilMs > 0 && millis() >= externalLedUntilMs) {
    digitalWrite(EXTERNAL_LED_PIN, LOW);
    externalLedUntilMs = 0;
  }
}

void updateMonitoringLed() {
  unsigned long now = millis();

  if (sosActive) {
    if (now < sosUntilMs) {
      setBuiltInRgb(255, 0, 0);
      return;
    }
    sosActive = false;
  }

  if (lowLight) {
    bool ledOn = ((now / LOW_LIGHT_BLINK_MS) % 2) == 0;
    if (ledOn) {
      setBuiltInRgb(255, 150, 0);
    } else {
      builtInOff();
    }
  } else {
    builtInOff();
  }
}

void setTrafficLights(bool redOn, bool yellowOn, bool greenOn) {
  digitalWrite(TRAFFIC_RED_PIN, redOn ? HIGH : LOW);
  digitalWrite(TRAFFIC_YELLOW_PIN, yellowOn ? HIGH : LOW);
  digitalWrite(TRAFFIC_GREEN_PIN, greenOn ? HIGH : LOW);
}

void updateTrafficChallenge() {
  unsigned long now = millis();

  if (lowLight) {
    pedestrianRequest = false;
    trafficState = TRAFFIC_GREEN;
    bool yellowOn = ((now / NIGHT_TRAFFIC_BLINK_MS) % 2) == 0;
    setTrafficLights(false, yellowOn, false);
    setBuiltInRgb(yellowOn ? 255 : 0, yellowOn ? 150 : 0, 0);
    return;
  }

  builtInOff();

  switch (trafficState) {
    case TRAFFIC_GREEN:
      setTrafficLights(false, false, true);
      if (pedestrianRequest) {
        pedestrianRequest = false;
        trafficState = TRAFFIC_YELLOW_TO_RED;
        trafficStateStartMs = now;
        Serial.println("Travessia solicitada: verde -> amarelo.");
      }
      break;

    case TRAFFIC_YELLOW_TO_RED:
      setTrafficLights(false, true, false);
      if (now - trafficStateStartMs >= 2000) {
        trafficState = TRAFFIC_RED_FOR_PEDESTRIAN;
        trafficStateStartMs = now;
        Serial.println("Semaforo vermelho para travessia.");
      }
      break;

    case TRAFFIC_RED_FOR_PEDESTRIAN:
      setTrafficLights(true, false, false);
      if (now - trafficStateStartMs >= 5000) {
        trafficState = TRAFFIC_GREEN;
        trafficStateStartMs = now;
        Serial.println("Fim da travessia: retorno ao verde.");
      }
      break;
  }
}

void setupWifiAndWebserver() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(ADC_BITS);

  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  digitalWrite(EXTERNAL_LED_PIN, LOW);

  pinMode(TRAFFIC_RED_PIN, OUTPUT);
  pinMode(TRAFFIC_YELLOW_PIN, OUTPUT);
  pinMode(TRAFFIC_GREEN_PIN, OUTPUT);
  setTrafficLights(false, false, false);

  rgb.begin();
  rgb.clear();
  rgb.show();

  attachInterrupt(digitalPinToInterrupt(SOS_BUTTON_PIN), onSosButtonInterrupt, FALLING);

  setupWifiAndWebserver();

  Serial.println("Sistema iniciado.");
  Serial.println("Ajuste LOW_LIGHT_THRESHOLD conforme os valores medidos no laboratorio.");
}

void loop() {
  server.handleClient();
  updateLdrReading();
  consumeSosInterruptIfNeeded();
  updateExternalLedPulse();

#if ENABLE_TRAFFIC_CHALLENGE
  updateTrafficChallenge();
#else
  updateMonitoringLed();
#endif
}
