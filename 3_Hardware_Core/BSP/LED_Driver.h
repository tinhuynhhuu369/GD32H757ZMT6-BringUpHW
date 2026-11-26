#ifndef __LED_DRIVER_H__
#define __LED_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#include "gd32h73x_75x.h"

/* LED pins (active high) */
#define LED_RED_PORT        GPIOF
#define LED_RED_PIN         GPIO_PIN_14
#define LED_RED_CLK         RCU_GPIOF

#define LED_YELLOW_PORT     GPIOG
#define LED_YELLOW_PIN      GPIO_PIN_13
#define LED_YELLOW_CLK      RCU_GPIOG

#define LED_GREEN_PORT      GPIOG
#define LED_GREEN_PIN       GPIO_PIN_12
#define LED_GREEN_CLK       RCU_GPIOG

typedef enum {
    LED_RED = 0,
    LED_YELLOW,
    LED_GREEN,
    LED_COUNT
} led_id_t;

void LED_Init(void);                                /* GPIO output, all off */
void LED_On(led_id_t id);                           /* stops blinking */
void LED_Off(led_id_t id);                          /* stops blinking */
void LED_Toggle(led_id_t id);
/* toggle every half_period_ms from the timer ISR; 0 = stop (LED keeps its level) */
void LED_Blink(led_id_t id, uint16_t half_period_ms);

/* called from TIMER6_IRQHandler every 1 ms */
void LED_Tick_1ms(void);

#endif /* __LED_DRIVER_H__ */
