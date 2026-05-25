// Definição dos Pinos (Ajuste conforme sua montagem)
const int PIN_VERDE = 8;
const int PIN_VERMELHO = 9;
const int PIN_AMARELO = 10;

// Lógica Invertida (Active Low):
// Como estamos lidando com LEDs possivelmente invertidos no ESP32:
#define LED_ON LOW
#define LED_OFF HIGH

// Variável para controlar se já passamos da fase de atenção
bool faseAtencaoConcluida = false;

void setup()
{
    pinMode(PIN_VERDE, OUTPUT);
    pinMode(PIN_VERMELHO, OUTPUT);
    pinMode(PIN_AMARELO, OUTPUT);

    // Inicia com todos desligados (Estado Seguro)
    digitalWrite(PIN_VERDE, LED_OFF);
    digitalWrite(PIN_VERMELHO, LED_OFF);
    digitalWrite(PIN_AMARELO, LED_OFF);

    Serial.begin(115200);
    Serial.println("=================================");
    Serial.println("  SISTEMA DE SEMÁFORO INICIADO   ");
    Serial.println("=================================");
}

void loop()
{

    // ==========================================
    // FASE 1: MODO ATENÇÃO (AMARELO PISCANTE)
    // ==========================================
    if (!faseAtencaoConcluida)
    {
        Serial.println("MODO ATENÇÃO: Iniciando Amarelo Piscante...");

        // Pisca o amarelo 5 vezes antes de ir para o ciclo normal
        for (int i = 0; i < 5; i++)
        {
            digitalWrite(PIN_AMARELO, LED_ON);
            delay(500); // Meio segundo aceso

            digitalWrite(PIN_AMARELO, LED_OFF);
            delay(500); // Meio segundo apagado

            Serial.print("Piscada ");
            Serial.println(i + 1);
        }

        // Marca a fase de atenção como concluída para nunca mais entrar neste "if"
        faseAtencaoConcluida = true;
        Serial.println("MODO ATENÇÃO: Concluído. Iniciando Ciclo Normal.");
        Serial.println("---------------------------------");
    }

    // FASE 2: CICLO NORMAL (Loop Infinito)
    // Estado 1: Verde (3 segundos)
    Serial.println("Sinal: VERDE");
    digitalWrite(PIN_VERDE, LED_ON);
    delay(3000);
    digitalWrite(PIN_VERDE, LED_OFF);

    // Estado 2: Vermelho (4 segundos)
    Serial.println("Sinal: VERMELHO");
    digitalWrite(PIN_VERMELHO, LED_ON);
    delay(4000);
    digitalWrite(PIN_VERMELHO, LED_OFF);

    // Estado 3: Amarelo (1 segundo)
    Serial.println("Sinal: AMARELO");
    digitalWrite(PIN_AMARELO, LED_ON);
    delay(1000);
    digitalWrite(PIN_AMARELO, LED_OFF);
}