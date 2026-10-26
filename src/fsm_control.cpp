#include "fsm_control.h"
#include <Arduino.h>
#include "hal_motor.h"
#include "boton_isr.h"

namespace {
enum class Estado : uint8_t { IDLE, RUNNING, FAULT, COUNT };
enum class Evento : uint8_t { NONE, BTN_PRESS, TIMEOUT, COUNT };

constexpr size_t N_ESTADOS = static_cast<size_t>(Estado::COUNT);
constexpr size_t N_EVENTOS = static_cast<size_t>(Evento::COUNT);

constexpr uint32_t DUTY_AVANCE = 700;   // de 1024, cerca del 68 %
constexpr uint32_t REPOSO_MS = 3000;

// Límite de tiempo por estado en milisegundos; 0 significa sin timeout.
constexpr uint32_t TIMEOUT_MS[N_ESTADOS] = {REPOSO_MS, 0, 0};

// Cada handler hace su acción sobre el motor y devuelve el estado siguiente.
using Transition = Estado (*)();

constexpr size_t idx(Estado e) { return static_cast<size_t>(e); }
constexpr size_t idx(Evento e) { return static_cast<size_t>(e); }

// Handlers de las celdas sin cambio: no hacen nada y conservan el estado.
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

// Matriz de transiciones: filas = estado, columnas = evento.
// Todas las celdas tienen handler, así que el despacho no necesita
// condicionales y siempre cuesta lo mismo, O(1).
constexpr Transition FSM[N_ESTADOS][N_EVENTOS] = {
    /* IDLE    */ {idleIgnora,    idleIgnora,    idleTimeout},
    /* RUNNING */ {runningIgnora, runningBoton,  runningIgnora},
    /* FAULT   */ {faultIgnora,   faultBoton,    faultIgnora},
};

Estado estado = Estado::IDLE;
uint32_t inicioEstadoMs = 0;  // instante en que se entró al estado actual

// Un solo evento por vuelta: la pulsación tiene prioridad sobre el timeout.
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
        inicioEstadoMs = millis();  // reinicia el conteo del timeout
    }
}
