#pragma once
#include <stdint.h>

constexpr uint32_t MOTOR_DUTY_MAX = 1024;

void motorInit();
void motorForward(uint32_t duty);
void motorBrake();
void motorCoast();