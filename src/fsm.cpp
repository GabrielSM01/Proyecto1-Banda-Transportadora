#include "fsm.h"
#include <Arduino.h>
#include "hal_motor.h"
#include "boton_isr.h"

namespace {
enum class Estado : uint8_t { IDLE, RUNNING, FAULT, COUNT };
enum class Evento : uint8_t { NONE, BTN_PRESS, TIMEOUT, COUNT };

constexpr size_t N_ESTADOS = static_cast<size_t>(Estado::COUNT);
constexpr size_t N_EVENTOS = static_cast<size_t>(Evento::COUNT);

constexpr uint32_t DUTY_AVANCE = 700;
constexpr uint32_t REPOSO_MS = 3000;

constexpr uint32_t TIMEOUT_MS[N_ESTADOS] = {REPOSO_MS, 0, 0};

using Transition = Estado (*)();

constexpr size_t idx(Estado e) { return static_cast<size_t>(e); }
constexpr size_t idx(Evento e) { return static_cast<size_t>(e); }

Estado idleIgnora()    { return Estado::IDLE; }
Estado runningIgnora() { return Estado::RUNNING; }
Estado faultIgnora()   { return Estado::FAULT; }

Estado idleTimeout()
{
    motorForward(DUTY_AVANCE);
    Serial.println("IDLE -> RUNNING");
    return Estado::RUNNING;
}

Estado runningBoton()
{
    motorBrake();
    Serial.printf("RUNNING -> FAULT (paro de emergencia, pulsaciones: %lu)\n", (unsigned long)botonContador());
    return Estado::FAULT;
}

Estado faultBoton()
{
    motorCoast();
    Serial.println("FAULT -> IDLE (rearme)");
    return Estado::IDLE;
}

constexpr Transition FSM[N_ESTADOS][N_EVENTOS] = {
    /* IDLE    */ {idleIgnora,    idleIgnora,    idleTimeout},
    /* RUNNING */ {runningIgnora, runningBoton,  runningIgnora},
    /* FAULT   */ {faultIgnora,   faultBoton,    faultIgnora},
};

Estado estado = Estado::IDLE;
uint32_t inicioEstadoMs = 0;

Evento leerEvento()
{
    if (botonTomarEvento()) return Evento::BTN_PRESS;
    uint32_t limite = TIMEOUT_MS[idx(estado)];
    if (limite != 0 && (uint32_t)(millis() - inicioEstadoMs) >= limite) return Evento::TIMEOUT;
    return Evento::NONE;
}
}

void fsmInit()
{
    estado = Estado::IDLE;
    inicioEstadoMs = millis();
    motorCoast();
}

void fsmActualizar()
{
    Evento e = leerEvento();
    Estado siguiente = FSM[idx(estado)][idx(e)]();
    if (siguiente != estado) {
        estado = siguiente;
        inicioEstadoMs = millis();
    }
}