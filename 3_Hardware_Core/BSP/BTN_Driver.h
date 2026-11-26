#ifndef __BTN_DRIVER_H__
#define __BTN_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#include "gd32h73x_75x.h"

/* ---------------------------------------------------------------------------
 * Front-panel buttons (LQFP144), active LOW, internal pull-up
 *
 *   Home    pin 56  PG0        Up      pin 15  PF5
 *   Return  pin 11  PF1        Down    pin 14  PF4
 *   OK      pin 12  PF2        Left    pin 82  PD13
 *                              Right   pin 55  PF15
 *
 * BTN_Tick_1ms() runs in TIMER6_IRQHandler: 20 ms debounce, one event per
 * press (released -> pressed edge). Events are read with BTN_GetPress().
 * ------------------------------------------------------------------------- */
#define BTN_HOME_PORT       GPIOG
#define BTN_HOME_PIN        GPIO_PIN_0
#define BTN_HOME_CLK        RCU_GPIOG

#define BTN_RETURN_PORT     GPIOF
#define BTN_RETURN_PIN      GPIO_PIN_1
#define BTN_RETURN_CLK      RCU_GPIOF

#define BTN_OK_PORT         GPIOF
#define BTN_OK_PIN          GPIO_PIN_2
#define BTN_OK_CLK          RCU_GPIOF

#define BTN_UP_PORT         GPIOF
#define BTN_UP_PIN          GPIO_PIN_5
#define BTN_UP_CLK          RCU_GPIOF

#define BTN_DOWN_PORT       GPIOF
#define BTN_DOWN_PIN        GPIO_PIN_4
#define BTN_DOWN_CLK        RCU_GPIOF

#define BTN_LEFT_PORT       GPIOD
#define BTN_LEFT_PIN        GPIO_PIN_13
#define BTN_LEFT_CLK        RCU_GPIOD

#define BTN_RIGHT_PORT      GPIOF
#define BTN_RIGHT_PIN       GPIO_PIN_15
#define BTN_RIGHT_CLK       RCU_GPIOF

/* the level must stay stable this long before the state changes */
#define BTN_DEBOUNCE_MS     20U
/* pending press events (power of two) */
#define BTN_QUEUE_SIZE      8U

typedef enum {
    BTN_HOME = 0,
    BTN_RETURN,
    BTN_OK,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_COUNT
} btn_id_t;

/* Debug (Live Watch): bit n = button n (btn_id_t), 1 = pressed */
extern volatile uint8_t  g_btn_raw;         /* last raw read, not debounced */
extern volatile uint8_t  g_btn_state;       /* debounced state */
extern volatile uint32_t g_btn_overflow;    /* presses dropped, queue full */

void BTN_Init(void);                /* GPIO input + pull-up, initial state */
bool BTN_GetPress(btn_id_t *id);    /* true if a press was pending (main loop) */
bool BTN_IsPressed(btn_id_t id);    /* debounced state */

/* called from TIMER6_IRQHandler every 1 ms */
void BTN_Tick_1ms(void);

#endif /* __BTN_DRIVER_H__ */
