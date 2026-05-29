// pinos dos LEDs
const int LED_BIT3 = 19;
const int LED_BIT2 = 18;
const int LED_BIT1 = 5;
const int LED_BIT0 = 17;

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BIT3, OUTPUT);
    pinMode(LED_BIT2, OUTPUT);
    pinMode(LED_BIT1, OUTPUT);
    pinMode(LED_BIT0, OUTPUT);
    Serial.println("Modo Complemento de 1: Digite 2 operandos de 4 bits separados por espaco (ex: 0101 1101)");
}

void loop()
{
    if (Serial.available() > 0)
    {
        // leitura via serial
        String input = Serial.readStringUntil('\n');
        input.trim();

        int espacoIdx = input.indexOf(' ');
        if (espacoIdx == -1)
            return;

        String strA = input.substring(0, espacoIdx);
        String strB = input.substring(espacoIdx + 1);

        // parsing sem sinal
        int valA = strtol(strA.c_str(), NULL, 2);
        int valB = strtol(strB.c_str(), NULL, 2);

        // soma
        int soma = valA + valB;

        // carry
        // Se a soma ultrapassar 15 (1111 binário), tem carry no MSB
        if (soma > 15)
        {
            // soma o carry no LSB
            soma = (soma & 0x0F) + 1;
        }

        // se o resultado for 1111, pode ser o "zero negativo" do complemento de 1

        // output para LEDs
        digitalWrite(LED_BIT0, soma & 0x01);
        digitalWrite(LED_BIT1, (soma >> 1) & 0x01);
        digitalWrite(LED_BIT2, (soma >> 2) & 0x01);
        digitalWrite(LED_BIT3, (soma >> 3) & 0x01);

        Serial.print("Resultado Binario nos LEDs: ");
        Serial.println(soma, BIN);
    }
}