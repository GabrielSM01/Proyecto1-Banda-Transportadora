#pragma once
#include <stddef.h>
#include <stdint.h>

// Estados y eventos de la máquina de estados de la banda transportadora.
enum class State : uint8_t {
    IDLE = 0,        // En espera, motor libre
    RUNNING,         // Motor en marcha
    FAULT_LOCKOUT,   // Bloqueo por paro de emergencia, motor frenado
    COUNT            // Cantidad de estados
};

enum class Event : uint8_t {
    NONE = 0,        // Sin evento
    START_CMD,       // Comando 's' por el monitor serie
    ESTOP_TRIPPED,   // Microinterruptor de paro de emergencia en IO33
    RESET_CMD,       // Comando 'r' por el monitor serie
    COUNT            // Cantidad de eventos
};

// Cada celda de la matriz guarda el siguiente estado y la acción a ejecutar.
struct Transition {
    State nextState;
    void (*action)();
};

extern const Transition FSM[static_cast<size_t>(State::COUNT)]
                           [static_cast<size_t>(Event::COUNT)];

// Acciones: se definen en main.cpp, donde están el motor y el puerto serie.
void onStart();
void onFault();
void onReset();
void onNoOp();

// Nombres para mostrar el estado y el evento en el monitor serie.
inline const char *nombreEstado(State s)
{
    switch (s) {
        case State::IDLE:          return "IDLE (0)";
        case State::RUNNING:       return "RUNNING (1)";
        case State::FAULT_LOCKOUT: return "FAULT_LOCKOUT (2)";
        default:                   return "DESCONOCIDO";
    }
}

inline const char *nombreEvento(Event e)
{
    switch (e) {
        case Event::NONE:          return "NONE (0)";
        case Event::START_CMD:     return "START_CMD (1)";
        case Event::ESTOP_TRIPPED: return "ESTOP_TRIPPED (2)";
        case Event::RESET_CMD:     return "RESET_CMD (3)";
        default:                   return "DESCONOCIDO";
    }
}
