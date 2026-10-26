#include "boton_isr.h"
#include <Arduino.h>
#include "xtensa/core-macros.h"

namespace {
constexpr uint8_t PIN_BOTON = 33;

// 4.000.000 ciclos son 25 ms solo con la CPU a 160 MHz.
constexpr uint32_t CICLOS_REBOTE = 4000000UL;
static_assert(F_CPU == 160000000L, "CICLOS_REBOTE equivale a 25 ms solo con la CPU a 160 MHz");

// Estado compartido entre la ISR y el bucle principal.
// El spinlock protege ambas variables y volatile evita que el compilador
// guarde su valor en un registro.
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool eventoPendiente = false;
volatile uint32_t contadorPulsos = 0;
uint32_t ultimoDisparo = 0;  // solo la ISR lo modifica

// IRAM_ATTR: la ISR debe poder ejecutarse aunque la caché de flash esté
// deshabilitada. No usa Serial, delay ni millis.
void IRAM_ATTR isrBoton()
{
    uint32_t ahora = XTHAL_GET_CCOUNT();

    // La resta sin signo de 32 bits da el tiempo correcto aunque CCOUNT
    // haya dado la vuelta entre los dos disparos.
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
    // Se resta el umbral para que la primera pulsación nunca se descarte.
    ultimoDisparo = XTHAL_GET_CCOUNT() - CICLOS_REBOTE;
    attachInterrupt(digitalPinToInterrupt(PIN_BOTON), isrBoton, FALLING);
}

bool botonTomarEvento()
{
    // Lectura y limpieza de la bandera dentro del spinlock.
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
