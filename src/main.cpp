#include <Arduino.h>
#include "xtensa/core-macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "fsm_core.h"
#include "motor_hal.h"

// Microinterruptor de paro de emergencia y antirrebote por ciclos de reloj.
constexpr uint8_t  PIN_ESTOP       = 33;
constexpr uint32_t DEBOUNCE_CYCLES = 4000000UL;  // 25 ms solo con la CPU a 160 MHz
static_assert(F_CPU == 160000000L, "DEBOUNCE_CYCLES equivale a 25 ms solo con la CPU a 160 MHz");

constexpr uint32_t DUTY_AVANCE = 700;            // de 1024, cerca del 68 %

// El Serial lo usan loop() y la tarea de telemetría: se protege con un mutex.
SemaphoreHandle_t serialMutex;

// Datos compartidos entre la ISR y loop(). El spinlock protege la bandera y el
// contador; volatile evita que el compilador los guarde en un registro.
portMUX_TYPE isrSpinlock = portMUX_INITIALIZER_UNLOCKED;
volatile bool     emergencyFlag = false;
volatile uint32_t pressCount = 0;
uint32_t          lastInterruptCycle = 0;        // solo la ISR lo modifica

// Estado actual: lo escribe loop() y lo lee la tarea de telemetría.
volatile State currentState = State::IDLE;

// IRAM_ATTR: la ISR debe poder ejecutarse aunque la caché de flash esté
// deshabilitada. No usa Serial, delay ni millis.
void IRAM_ATTR emergencyStopISR()
{
    uint32_t now = XTHAL_GET_CCOUNT();

    // La resta sin signo da el tiempo correcto aunque CCOUNT haya dado la vuelta.
    if ((uint32_t)(now - lastInterruptCycle) < DEBOUNCE_CYCLES) return;
    lastInterruptCycle = now;

    portENTER_CRITICAL_ISR(&isrSpinlock);
    emergencyFlag = true;
    pressCount = pressCount + 1;
    portEXIT_CRITICAL_ISR(&isrSpinlock);
}

// Lee y limpia la bandera, y lee el contador, dentro de un mismo spinlock.
bool takeEmergency(uint32_t *total)
{
    bool hubo;
    portENTER_CRITICAL(&isrSpinlock);
    hubo = emergencyFlag;
    emergencyFlag = false;
    *total = pressCount;
    portEXIT_CRITICAL(&isrSpinlock);
    return hubo;
}

// Imprime una línea tomando el mutex del Serial.
void imprimir(const char *texto)
{
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println(texto);
        xSemaphoreGive(serialMutex);
    }
}

// Acciones de la máquina de estados.
void onStart()
{
    motorForward(DUTY_AVANCE);
    imprimir("[Accion] Motor encendido: banda transportadora en marcha.");
}

void onFault()
{
    motorBrake();  // freno dinámico: el eje se detiene de inmediato
    imprimir("[Accion] FALLA CRITICA: paro de emergencia activado. Motor frenado.");
}

void onReset()
{
    motorCoast();  // suelta el freno: el motor queda libre
    imprimir("[Accion] Sistema reiniciado y listo en espera.");
}

void onNoOp() {}

// Tarea de telemetría: imprime el estado cada 500 ms con prioridad baja.
void telemetryTask(void *pvParameters)
{
    (void)pvParameters;
    char linea[64];
    while (true) {
        snprintf(linea, sizeof(linea), "[Telemetria] Estado actual: %s", nombreEstado(currentState));
        imprimir(linea);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Un solo evento por vuelta: el paro de emergencia tiene prioridad sobre los comandos.
Event leerEvento()
{
    uint32_t total;
    if (takeEmergency(&total)) {
        char linea[64];
        snprintf(linea, sizeof(linea), "[Boton] Disparos aceptados por el filtro: %lu", (unsigned long)total);
        imprimir(linea);
        return Event::ESTOP_TRIPPED;
    }

    if (Serial.available()) {
        char c = tolower(Serial.read());
        if (c == 's') return Event::START_CMD;
        if (c == 'r') return Event::RESET_CMD;
    }
    return Event::NONE;
}

void setup()
{
    Serial.begin(115200);
    motorInit();

    serialMutex = xSemaphoreCreateMutex();

    pinMode(PIN_ESTOP, INPUT_PULLUP);
    // Se resta el umbral para que la primera pulsación nunca se descarte.
    lastInterruptCycle = XTHAL_GET_CCOUNT() - DEBOUNCE_CYCLES;
    attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), emergencyStopISR, FALLING);

    char linea[128];
    snprintf(linea, sizeof(linea), "[Inicio] Estado inicial: %s | 's' arranca, 'r' reinicia, boton IO33 = paro de emergencia",
             nombreEstado(currentState));
    imprimir(linea);

    xTaskCreatePinnedToCore(telemetryTask, "Telemetry", 3072, NULL, 1, NULL, 1);
}

void loop()
{
    Event evento = leerEvento();

    if (evento != Event::NONE) {
        State estadoInicial = currentState;
        const Transition &t = FSM[static_cast<size_t>(estadoInicial)]
                                 [static_cast<size_t>(evento)];

        char linea[160];
        snprintf(linea, sizeof(linea), "[FSM] Estado inicial: %s | Evento: %s -> Estado siguiente: %s",
                 nombreEstado(estadoInicial), nombreEvento(evento), nombreEstado(t.nextState));
        imprimir(linea);

        currentState = t.nextState;
        t.action();
    }

    // Cede el procesador al planificador de FreeRTOS.
    vTaskDelay(pdMS_TO_TICKS(10));
}
