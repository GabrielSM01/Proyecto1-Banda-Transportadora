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

void loop()
{
    fsmActualizar();
}