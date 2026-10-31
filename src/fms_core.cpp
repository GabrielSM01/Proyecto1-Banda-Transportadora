#include "fsm_core.h"

// Matriz de transiciones: filas = estado actual, columnas = evento.
// Todas las celdas tienen acción (onNoOp cuando no pasa nada), así que la
// consulta FSM[estado][evento] siempre cuesta lo mismo, O(1).
constexpr Transition FSM[static_cast<size_t>(State::COUNT)]
                        [static_cast<size_t>(Event::COUNT)] = {
    // IDLE
    {
        {State::IDLE,          onNoOp},    // NONE
        {State::RUNNING,       onStart},   // START_CMD: arranca el motor
        {State::IDLE,          onNoOp},    // ESTOP_TRIPPED: se ignora
        {State::IDLE,          onNoOp}     // RESET_CMD: se ignora
    },
    // RUNNING
    {
        {State::RUNNING,       onNoOp},    // NONE
        {State::RUNNING,       onNoOp},    // START_CMD: ya está en marcha
        {State::FAULT_LOCKOUT, onFault},   // ESTOP_TRIPPED: frena y bloquea
        {State::RUNNING,       onNoOp}     // RESET_CMD: se ignora
    },
    // FAULT_LOCKOUT
    {
        {State::FAULT_LOCKOUT, onNoOp},    // NONE
        {State::FAULT_LOCKOUT, onNoOp},    // START_CMD: bloqueado, se ignora
        {State::FAULT_LOCKOUT, onNoOp},    // ESTOP_TRIPPED: un rebote no rearma
        {State::IDLE,          onReset}    // RESET_CMD: libera el bloqueo
    }
};
