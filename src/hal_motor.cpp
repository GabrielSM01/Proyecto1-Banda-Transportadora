#include "hal_motor.h"
#include "driver/ledc.h"

namespace {
constexpr gpio_num_t PIN_IN_A = GPIO_NUM_12;
constexpr gpio_num_t PIN_IN_B = GPIO_NUM_14;
constexpr ledc_mode_t MODE = LEDC_HIGH_SPEED_MODE;
constexpr ledc_timer_t TIMER = LEDC_TIMER_0;
constexpr ledc_channel_t CH_A = LEDC_CHANNEL_0;
constexpr ledc_channel_t CH_B = LEDC_CHANNEL_1;

void setDuty(ledc_channel_t ch, uint32_t duty)
{
    ledc_set_duty(MODE, ch, duty);
    ledc_update_duty(MODE, ch);
}
}

void motorInit()
{
    ledc_timer_config_t timer = {};
    timer.speed_mode = MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = TIMER;
    timer.freq_hz = 1000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {};
    ch.speed_mode = MODE;
    ch.timer_sel = TIMER;
    ch.duty = 0;
    ch.hpoint = 0;

    ch.gpio_num = PIN_IN_A;
    ch.channel = CH_A;
    ledc_channel_config(&ch);

    ch.gpio_num = PIN_IN_B;
    ch.channel = CH_B;
    ledc_channel_config(&ch);
}

void motorForward(uint32_t duty)
{
    if (duty > MOTOR_DUTY_MAX) duty = MOTOR_DUTY_MAX;
    setDuty(CH_B, 0);
    setDuty(CH_A, duty);
}

void motorBrake()
{
    setDuty(CH_A, MOTOR_DUTY_MAX);
    setDuty(CH_B, MOTOR_DUTY_MAX);
}

void motorCoast()
{
    setDuty(CH_A, 0);
    setDuty(CH_B, 0);
}