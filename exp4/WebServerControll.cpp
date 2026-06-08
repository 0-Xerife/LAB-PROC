#include <WiFi.h>
#include <WebServer.h>

// config de rede
const char *ssid = "Manipulador_de_almas";
const char *password = "121216";

WebServer server(80);

// mapeamento dos pinos (verificar pinout da placa)
const int ledPin = 3;
const int servoPin = 4;

// Configurações dos Canais PWM (LEDC)
// Canal 0: LED (Alta Frequência, ajustável via Web)
const int ledChannel = 0;
int ledFreq = 1000;          // Inicial em 1 kHz
const int ledResolution = 8; // 8 bits (0 a 255)

// Canal 1: Servomotor (Baixa Frequência fixa em 50Hz)
const int servoChannel = 1;
const int servoFreq = 50;       // 50Hz = período de 20ms
const int servoResolution = 12; // 12 bits (0 a 4095) para maior precisão

// interface web
// O JS usa fetch() para chamadas HTTP assíncronas sem recarregar a página.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Painel ESP32-C3 - Lab 5</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #eef2f5; padding: 15px; margin: 0; }
        .container { max-width: 450px; margin: auto; background: white; padding: 25px; border-radius: 12px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }
        h2 { color: #2c3e50; margin-bottom: 25px; }
        .slider-group { margin: 20px 0; text-align: left; background: #f8f9fa; padding: 15px; border-radius: 8px; border: 1px solid #ddd; }
        label { font-weight: bold; color: #34495e; font-size: 1.1em; }
        input[type=range] { width: 100%; margin-top: 15px; cursor: pointer; height: 8px; background: #bdc3c7; border-radius: 5px; outline: none; }
        span.val { color: #e74c3c; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h2>Controle de Periféricos</h2>
        
        <div class="slider-group">
            <label>Brilho do LED: <span id="ledVal" class="val">0</span>%</label>
            <input type="range" id="ledSlider" min="0" max="100" value="0" oninput="updateESP()">
        </div>

        <div class="slider-group">
            <label>Frequência (LED): <span id="freqVal" class="val">1000</span> Hz</label>
            <input type="range" id="freqSlider" min="50" max="5000" step="50" value="1000" oninput="updateESP()">
        </div>

        <div class="slider-group">
            <label>Posição do Servo: <span id="servoVal" class="val">0</span>&deg;</label>
            <input type="range" id="servoSlider" min="0" max="180" value="0" oninput="updateESP()">
        </div>
    </div>

    <script>
        function updateESP() {
            let led = document.getElementById("ledSlider").value;
            let freq = document.getElementById("freqSlider").value;
            let servo = document.getElementById("servoSlider").value;

            // Atualiza a interface gráfica em tempo real
            document.getElementById("ledVal").innerText = led;
            document.getElementById("freqVal").innerText = freq;
            document.getElementById("servoVal").innerText = servo;

            // Envia requisição GET assíncrona (AJAX) para o ESP32
            fetch(`/api/update?led=${led}&freq=${freq}&servo=${servo}`);
        }
    </script>
</body>
</html>
)rawliteral";

// requisicoes para alteracao
void handleUpdate()
{
    if (server.hasArg("led") && server.hasArg("freq") && server.hasArg("servo"))
    {
        int ledPercent = server.arg("led").toInt();
        int newFreq = server.arg("freq").toInt();
        int servoAngle = server.arg("servo").toInt();

        // controle da frequencia do led
        if (newFreq != ledFreq)
        {
            ledFreq = newFreq;
            // reconfiguracao em caso de slide
            ledcSetup(ledChannel, ledFreq, ledResolution);
        }

        // controle do duty cycle do led (0 a 100% convertido pra 0 a 255)
        int ledDuty = map(ledPercent, 0, 100, 0, 255);
        ledcWrite(ledChannel, ledDuty);

        // controle do servomotor (matematica baseada no PDF)
        // Resolução de 12 bits = 4096 valores (0 a 4095).
        // A 50Hz, 1 ciclo = 20ms.
        // - 1.0 ms (0 graus)   = (1.0 / 20) * 4096 = ~205
        // - 2.0 ms (180 graus) = (2.0 / 20) * 4096 = ~410
        int servoDuty = map(servoAngle, 0, 180, 205, 410);
        ledcWrite(servoChannel, servoDuty);

        server.send(200, "text/plain", "OK");
    }
    else
    {
        server.send(400, "text/plain", "Bad Request");
    }
}

// setup e loop principal
void setup()
{
    Serial.begin(115200);
    delay(100);

    // configuracao do hardware pwm (ledc)
    // Nota: Em versões do Core ESP32 Arduino >= 3.0, ledcSetup foi descontinuada.
    // Se houver erro de compilação, precisará usar ledcAttach(pin, freq, resolution);
    ledcSetup(ledChannel, ledFreq, ledResolution);
    ledcAttachPin(ledPin, ledChannel);

    ledcSetup(servoChannel, servoFreq, servoResolution);
    ledcAttachPin(servoPin, servoChannel);

    // inicializando posicoes padrao
    ledcWrite(ledChannel, 0);
    ledcWrite(servoChannel, map(0, 0, 180, 205, 410)); // Força posição 0 no boot

    // conexao com wifi
    Serial.println("\nConectando ao Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConectado com sucesso!");
    Serial.print("Acesse o painel no navegador via IP: ");
    Serial.println(WiFi.localIP());

    // roteamento de uris do servidor
    server.on("/", []()
              { server.send(200, "text/html", index_html); });
    server.on("/api/update", handleUpdate);

    server.begin();
}

void loop()
{
    // apenas ouve o cliente (sem travar a cpu num while-loop)
    server.handleClient();
}