// Precisou ser feita uma alteração inteira no codigo referente ao antigo por conta de erro de consideração do grupo em relação ao LED do esp32.

// O código antigo considerava 3 LEDs distintos, enquanto esse mais atualizado considera tanto o LED ser RGB, quanto ser um LED só.


#include <Adafruit_NeoPixel.h>

#define PIN_LED 8      // Pino de dados
#define NUM_LEDS 1

Adafruit_NeoPixel led(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

bool faseAtencaoConcluida = false;

void setup()
{
    Serial.begin(115200);

    led.begin();
    led.clear();
    led.show();

    Serial.println("SISTEMA DE SEMAFORO INICIADO");
}

void setColor(int r, int g, int b)
{
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}

void loop()
{
    // FASE 1: Amarelo piscante
    if (!faseAtencaoConcluida)
    {
        Serial.println("MODO ATENCAO");

        for (int i = 0; i < 5; i++)
        {
            // Amarelo
            setColor(255, 255, 0);
            delay(700);

            // Desliga
            setColor(0, 0, 0);
            delay(700);
        }

        faseAtencaoConcluida = true;
    }

    // VERDE
    Serial.println("VERDE");
    setColor(0, 255, 0);
    delay(3000);

    // DESLIGA
    setColor(0, 0, 0);

    // VERMELHO
    Serial.println("VERMELHO");
    setColor(255, 0, 0);
    delay(4000);

    // DESLIGA
    setColor(0, 0, 0);

    // AMARELO
    Serial.println("AMARELO");
    setColor(255, 255, 0);
    delay(1000);

    // DESLIGA
    setColor(0, 0, 0);
}
