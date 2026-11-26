#ifndef __TIMER_TICK_DRIVER_H__
#define __TIMER_TICK_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#include "gd32h73x_75x.h"

/* ---------------------------------------------------------------------------
 * 1 ms time base on a basic timer (TIMER6, APB1 domain)
 *
 *   CK_TIMER --PSC--> 1 MHz counter --CAR=999--> update event every 1 ms
 *
 * TIMER6_IRQHandler (gd32h73x_75x_it.c) clears the update flag and calls
 * TMR_Tick_Increment() and LED_Tick_1ms().
 * ------------------------------------------------------------------------- */
#define TMR_TICK_TIMER          TIMER6
#define TMR_TICK_CLK            RCU_TIMER6
#define TMR_TICK_IRQn           TIMER6_IRQn
#define TMR_TICK_IRQ_PRIORITY   1U          /* below SysTick (0) */

#define TMR_COUNTER_FREQ_HZ     1000000U    /* counter clock after prescaler */
#define TMR_TICK_FREQ_HZ        1000U       /* update interrupt rate */

/* Debug: timer input clock actually measured from RCU (see Live Watch) */
extern volatile uint32_t g_tmr_clk_hz;

void     TMR_Init(void);
uint32_t TMR_GetTick(void);

/* CK_TIMER of the APB1 timers (TIMER1..6, 22, 23, 30, 31, 50, 51) in Hz,
   computed from the RCU settings */
uint32_t TMR_GetApb1TimerClock(void);

/* true once period_ms has passed since *last, then *last is advanced.
   Non-blocking: call it repeatedly from the main loop. */
bool     TMR_Elapsed(uint32_t *last, uint32_t period_ms);

/* blocking wait based on the timer tick. Use only where the hardware
   requires a fixed wait (LCD reset / sleep-out during init). */
void     TMR_DelayMs(uint32_t ms);

/* called from TIMER6_IRQHandler only */
void     TMR_Tick_Increment(void);

#endif /* __TIMER_TICK_DRIVER_H__ */
