/*!
    \file    LED_Driver.c
    \brief   status LEDs, blinking driven by the 1 ms timer tick (no delay)
*/

#include "LED_Driver.h"

typedef struct {
    uint32_t port;
    uint32_t pin;
    rcu_periph_enum clk;
} led_hw_t;

static const led_hw_t s_led_hw[LED_COUNT] = {
    [LED_RED]    = {LED_RED_PORT,    LED_RED_PIN,    LED_RED_CLK},
    [LED_YELLOW] = {LED_YELLOW_PORT, LED_YELLOW_PIN, LED_YELLOW_CLK},
    [LED_GREEN]  = {LED_GREEN_PORT,  LED_GREEN_PIN,  LED_GREEN_CLK},
};

/* written by main (LED_Blink/On/Off), read by the ISR */
static volatile uint16_t s_blink_period[LED_COUNT];
/* ISR only */
static uint16_t          s_blink_cnt[LED_COUNT];

void LED_Init(void)
{
    for (uint32_t i = 0U; i < LED_COUNT; i++) {
        rcu_periph_clock_enable(s_led_hw[i].clk);
        gpio_bit_reset(s_led_hw[i].port, s_led_hw[i].pin);
        gpio_mode_set(s_led_hw[i].port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, s_led_hw[i].pin);
        gpio_output_options_set(s_led_hw[i].port, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, s_led_hw[i].pin);
        s_blink_period[i] = 0U;
        s_blink_cnt[i]    = 0U;
    }
}

void LED_On(led_id_t id)
{
    s_blink_period[id] = 0U;
    gpio_bit_set(s_led_hw[id].port, s_led_hw[id].pin);
}

void LED_Off(led_id_t id)
{
    s_blink_period[id] = 0U;
    gpio_bit_reset(s_led_hw[id].port, s_led_hw[id].pin);
}

void LED_Toggle(led_id_t id)
{
    gpio_bit_toggle(s_led_hw[id].port, s_led_hw[id].pin);
}

void LED_Blink(led_id_t id, uint16_t half_period_ms)
{
    s_blink_period[id] = half_period_ms;
}

void LED_Tick_1ms(void)
{
    for (uint32_t i = 0U; i < LED_COUNT; i++) {
        uint16_t period = s_blink_period[i];

        if (0U == period) {
            s_blink_cnt[i] = 0U;
            continue;
        }
        if (++s_blink_cnt[i] >= period) {
            s_blink_cnt[i] = 0U;
            gpio_bit_toggle(s_led_hw[i].port, s_led_hw[i].pin);
        }
    }
}
