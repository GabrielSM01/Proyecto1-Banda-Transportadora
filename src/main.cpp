#include <Arduino.h>
#include "hal_motor.h"
#include "boton_isr.h"

void setup()
{
    Serial.begin(115200);
    motorInit();
    motorCoast();
    botonInit();
}

void loop()
{
    if (botonTomarEvento()) {
        Serial.printf("Pulsacion %lu\n", (unsigned long)botonContador());
    }
}