#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>

// Pinos dos LEDs no ESP32
// TROCAR OS PINOS PARA PINOS USUAIS NO ESP32
const int LED_BIT3 = 19;
const int LED_BIT2 = 18;
const int LED_BIT1 = 5;
const int LED_BIT0 = 17;

// Nome da rede no wifi
const char *ssid = "Calc_Blaster_Master";

NetworkServer server(80);

// HTML com JS embutido
const char *html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Calculadora Binaria</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        button { margin: 5px; padding: 10px; cursor: pointer; }
        .resultado-box { margin-top: 20px; padding: 15px; border: 1px solid #ccc; background: #f9f9f9; }
    </style>
</head>
<body>
    <h2>Calculadora Multi-bits ESP32</h2>
    <p>Insira os valores em binário (o tamanho do bit ditará o sinal em complemento de 2):</p>
    Operando A: <input type="text" id="valA" value="00000110"><br><br>
    Operando B: <input type="text" id="valB" value="00000010"><br><br>
    
    <button onclick="calcular('add')">SOMA (+)</button>
    <button onclick="calcular('sub')">SUBTRAÇÃO (-)</button>
    <button onclick="calcular('mul')">MULTIPLICAÇÃO (*)</button>
    <button onclick="calcular('div')">DIVISÃO (/)</button>
    <button onclick="calcular('fat')">FATORIAL de A (!)</button>
    <br><br>
    
    <div class="resultado-box">
        <b>Resultado:</b>
        <p id="resultado">Aguardando operação...</p>
    </div>
    
    <script>
        function calcular(op) {
            let a = document.getElementById('valA').value;
            let b = document.getElementById('valB').value;
            if(!a) a = "0";
            if(!b) b = "0";
            
            document.getElementById('resultado').innerHTML = "Calculando...";
            
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

// Função para interpretar binário de tamanho dinâmico (Complemento de 2)
long parseBinarySigned(String binStr)
{
    long val = strtol(binStr.c_str(), NULL, 2);
    int len = binStr.length();
    // Se o bit mais significativo for 1, o número é negativo
    if (len > 0 && binStr.charAt(0) == '1')
    {
        val -= (1 << len);
    }
    return val;
}

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
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        if (header.indexOf("GET /calc") >= 0)
                        {
                            int a_idx = header.indexOf("a=");
                            int b_idx = header.indexOf("&b=");
                            int op_idx = header.indexOf("&op=");
                            int fim_idx = header.indexOf(" HTTP");

                            String paramA = header.substring(a_idx + 2, b_idx);
                            String paramB = header.substring(b_idx + 3, op_idx);
                            String op = header.substring(op_idx + 4, fim_idx);

                            long valA = parseBinarySigned(paramA);
                            long valB = parseBinarySigned(paramB);
                            long res_decimal = 0;

                            bool erro = false;
                            String msg_erro = "";

                            // INICIO DE MEDIÇÃO DE TEMPO REQUISITO 6
                            unsigned long t_start = micros();

                            if (op == "add")
                            {
                                res_decimal = valA + valB;
                            }
                            else if (op == "sub")
                            {
                                res_decimal = valA - valB;
                            }
                            else if (op == "mul")
                            {
                                // REQUISITO 3 e 5: Multiplicação
                                long a = abs(valA);
                                long b = abs(valB);
                                res_decimal = 0;
                                // Critério de parada: quando o multiplicador (b) for zero
                                while (b > 0)
                                {
                                    if (b & 1)
                                        res_decimal += a;
                                    a <<= 1;
                                    b >>= 1;
                                }
                                if ((valA < 0 && valB > 0) || (valA > 0 && valB < 0))
                                {
                                    res_decimal = -res_decimal;
                                }
                            }
                            else if (op == "fat")
                            {
                                // REQUISITO 4 e 5: Fatorial
                                if (valA < 0)
                                {
                                    erro = true;
                                    msg_erro = "Fatorial não definido para negativos.";
                                }
                                else
                                {
                                    res_decimal = 1;
                                    // condicao para parada
                                    for (long i = 1; i <= valA; i++)
                                    {
                                        res_decimal *= i;
                                    }
                                }
                            }
                            else if (op == "div")
                            {
                                // REQUISITO 7 (Desafio): Divisão por subtrações sucessivas
                                if (valB == 0)
                                {
                                    erro = true;
                                    msg_erro = "Divisão por Zero!";
                                }
                                else
                                {
                                    long a = abs(valA);
                                    long b = abs(valB);
                                    res_decimal = 0;
                                    // Critério de parada: dividendo menor que o divisor
                                    while (a >= b)
                                    {
                                        a -= b;
                                        res_decimal++;
                                    }
                                    if ((valA < 0 && valB > 0) || (valA > 0 && valB < 0))
                                    {
                                        res_decimal = -res_decimal;
                                    }
                                }
                            }

                            // FIM DO CALCULO DE TEMPO
                            unsigned long t_end = micros();
                            unsigned long tempo_exec = t_end - t_start;

                            // Atualização dos LEDs (mantém o Requisito 1 e 2 - Mostra apenas os 4 LSBs)
                            int saida_leds = res_decimal & 0x0F;
                            digitalWrite(LED_BIT0, saida_leds & 0x01);
                            digitalWrite(LED_BIT1, (saida_leds >> 1) & 0x01);
                            digitalWrite(LED_BIT2, (saida_leds >> 2) & 0x01);
                            digitalWrite(LED_BIT3, (saida_leds >> 3) & 0x01);

                            // Respostas do front end
                            if (erro)
                            {
                                client.print("<span style='color:red; font-weight:bold;'>" + msg_erro + "</span>");
                            }
                            else
                            {
                                client.print("Valor Decimal: <b>" + String(res_decimal) + "</b><br>");
                                client.print("Binário Bruto (ESP32): <b>" + String(res_decimal, BIN) + "</b><br><br>");
                                client.print("<span style='color:blue;'>Tempo de processamento (C no ESP32): <b>" + String(tempo_exec) + " microssegundos (&mu;s)</b></span>");
                            }
                        }
                        else
                        {
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
}