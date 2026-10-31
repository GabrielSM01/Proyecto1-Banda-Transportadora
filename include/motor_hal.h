#pragma once
#include <stdint.h>

// Capa de abstracción del motor (puente H L9110S en IO12 e IO14).
// El ciclo de trabajo va de 0 a 1024 (PWM de 10 bits).
constexpr uint32_t MOTOR_DUTY_MAX = 1024;

void motorInit();               // configura el temporizador y los canales LEDC
void motorForward(uint32_t duty);  // avance con PWM
void motorBrake();              // freno dinámico (ambas entradas al 100 %)
void motorCoast();              // giro por inercia (ambas entradas en 0)
