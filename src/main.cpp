#include <Arduino.h>
#include "hal_motor.h"
#include "boton_isr.h"
#include "fsm_control.h"

void setup()
{
    Serial.begin(115200);
    motorInit();
    botonInit();
    fsmInit();
}

// El bucle no bloquea: cada vuelta lee un evento y lo despacha.
void loop()
{
    fsmActualizar();
}
