#include "boton_isr.h"
#include <Arduino.h>
#include "xtensa/core-macros.h"

namespace {
constexpr uint8_t PIN_BOTON = 33;
constexpr uint32_t CICLOS_REBOTE = 4000000UL;

static_assert(F_CPU == 160000000L, "CICLOS_REBOTE equivale a 25 ms solo con la CPU a 160 MHz");

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool eventoPendiente = false;
volatile uint32_t contadorPulsos = 0;
uint32_t ultimoDisparo = 0;

void IRAM_ATTR isrBoton()
{
    uint32_t ahora = XTHAL_GET_CCOUNT();
    if ((uint32_t)(ahora - ultimoDisparo) < CICLOS_REBOTE) return;
    ultimoDisparo = ahora;

    portENTER_CRITICAL_ISR(&mux);
    eventoPendiente = true;
    contadorPulsos = contadorPulsos + 1;
    portEXIT_CRITICAL_ISR(&mux);
}
}

void botonInit()
{
    pinMode(PIN_BOTON, INPUT_PULLUP);
    ultimoDisparo = XTHAL_GET_CCOUNT() - CICLOS_REBOTE;
    attachInterrupt(digitalPinToInterrupt(PIN_BOTON), isrBoton, FALLING);
}

bool botonTomarEvento()
{
    bool hubo;
    portENTER_CRITICAL(&mux);
    hubo = eventoPendiente;
    eventoPendiente = false;
    portEXIT_CRITICAL(&mux);
    return hubo;
}

uint32_t botonContador()
{
    uint32_t n;
    portENTER_CRITICAL(&mux);
    n = contadorPulsos;
    portEXIT_CRITICAL(&mux);
    return n;
}