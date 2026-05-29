#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>

// pinos dos LEDs no ESP32
const int LED_BIT3 = 19; // MSB (Sinal)
const int LED_BIT2 = 18;
const int LED_BIT1 = 5;
const int LED_BIT0 = 17; // LSB

const char *ssid = "Calculadora_ESP32";

NetworkServer server(80);

// HTML com JS embutido (Requisito 4)
const char *html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Calculadora Binaria</title></head>
<body>
    <h2>Calculadora 4-bits ESP32</h2>
    Operando A: <input type="text" id="valA" maxlength="4" value="0110"><br><br>
    Operando B: <input type="text" id="valB" maxlength="4" value="0010"><br><br>
    <button onclick="calcular('add')">SOMA</button>
    <button onclick="calcular('sub')">SUB</button>
    <br><br>
    <p id="resultado"></p>
    
    <script>
        function calcular(op) {
            let a = document.getElementById('valA').value;
            let b = document.getElementById('valB').value;
            // Envia os dados na URL da requisição HTTP GET
            fetch(`/calc?a=${a}&b=${b}&op=${op}`)
                .then(response => response.text())
                .then(data => {
                    document.getElementById('resultado').innerHTML = data;
                });
        }
    </script>
</body>
</html>
)rawliteral";

void setup()
{
    pinMode(LED_BIT3, OUTPUT);
    pinMode(LED_BIT2, OUTPUT);
    pinMode(LED_BIT1, OUTPUT);
    pinMode(LED_BIT0, OUTPUT);

    Serial.begin(115200);
    Serial.println("Configurando access point...");

    if (!WiFi.softAP(ssid))
    {
        log_e("Soft AP creation failed.");
        while (1)
            ;
    }

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    server.begin();
    Serial.println("Server started");
}

void loop()
{
    NetworkClient client = server.accept(); // listen for incoming clients

    if (client)
    {
        String currentLine = "";
        String header = ""; // variável para armazenar a requisição inteira do navegador

        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                header += c; // vai gravando tudo que o navegador manda

                if (c == '\n')
                {
                    // fim da requisição HTTP do navegador
                    if (currentLine.length() == 0)
                    {
                        // envia o cabeçalho HTTP padrão
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        // se a requisição contiver "/calc", o usuário apertou o botão
                        if (header.indexOf("GET /calc") >= 0)
                        {

                            // extrai os operandos da URL (Ex: /calc?a=0110&b=0010&op=add)
                            int a_idx = header.indexOf("a=");
                            int b_idx = header.indexOf("&b=");
                            int op_idx = header.indexOf("&op=");
                            int fim_idx = header.indexOf(" HTTP"); // Fim da requisição GET

                            String paramA = header.substring(a_idx + 2, b_idx);
                            String paramB = header.substring(b_idx + 3, op_idx);
                            String op = header.substring(op_idx + 4, fim_idx);

                            // REQUISITO 5: calculo em C
                            int valA = strtol(paramA.c_str(), NULL, 2);
                            int valB = strtol(paramB.c_str(), NULL, 2);

                            // ajuste de complemento de dois
                            if (valA >= 8)
                                valA -= 16;
                            if (valB >= 8)
                                valB -= 16;

                            int res_decimal = (op == "add") ? (valA + valB) : (valA - valB);

                            // deteccao de overflow
                            bool overflow = false;
                            if (res_decimal > 7 || res_decimal < -8)
                            {
                                overflow = true;
                            }

                            // mascara para garantir apenas os 4 bits da saída
                            int saida = res_decimal & 0x0F;
                            digitalWrite(LED_BIT0, saida & 0x01);
                            digitalWrite(LED_BIT1, (saida >> 1) & 0x01);
                            digitalWrite(LED_BIT2, (saida >> 2) & 0x01);
                            digitalWrite(LED_BIT3, (saida >> 3) & 0x01);

                            // responde para o Javascript embutido na página
                            if (overflow)
                            {
                                client.print("<span style='color:red; font-weight:bold;'>OVERFLOW!</span>");
                            }
                            else
                            {
                                client.print("<span style='color:green;'>Sinal enviado aos LEDs!</span>");
                            }
                        }
                        // se não tiver "/calc", envia a página inicial (Front-End)
                        else
                        {
                            client.print(html_page);
                        }

                        // encerra a transmissão
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
}