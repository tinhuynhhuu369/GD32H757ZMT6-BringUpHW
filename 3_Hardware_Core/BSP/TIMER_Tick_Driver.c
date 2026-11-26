/*!
    \file    TIMER_Tick_Driver.c
    \brief   1 ms time base on TIMER6 (replaces busy-wait delays for LCD / LED)
*/

#include "TIMER_Tick_Driver.h"

static volatile uint32_t s_tick_ms = 0U;
volatile uint32_t        g_tmr_clk_hz = 0U;

/* ---------------------------------------------------------------------------
 * CK_TIMER for the APB1 domain (TIMER6). Rules from RCU_CFG1.TIMERSEL:
 *   TIMERSEL = 0 (x2): APB1PSC <= /2  -> CK_TIMER = CK_AHB, else 2 x CK_APB1
 *   TIMERSEL = 1 (x4): APB1PSC <= /4  -> CK_TIMER = CK_AHB, else 4 x CK_APB1
 * With the 600 MHz config (AHB = 300 MHz, APB1 = AHB/2): CK_TIMER = 300 MHz.
 * ------------------------------------------------------------------------- */
static uint32_t TMR_GetApb1TimerClock(void)
{
    uint32_t ahb  = rcu_clock_freq_get(CK_AHB);
    uint32_t apb1 = rcu_clock_freq_get(CK_APB1);
    uint32_t psc  = (RCU_CFG0 & RCU_CFG0_APB1PSC) >> 10U;   /* 0..3 = /1, 4 = /2, 5 = /4 ... */

    if (0U != (RCU_CFG1 & RCU_CFG1_TIMERSEL)) {
        return (psc <= 5U) ? ahb : (apb1 * 4U);
    } else {
        return (psc <= 4U) ? ahb : (apb1 * 2U);
    }
}

void TMR_Init(void)
{
    timer_parameter_struct timer_initpara;

    g_tmr_clk_hz = TMR_GetApb1TimerClock();

    rcu_periph_clock_enable(TMR_TICK_CLK);
    timer_deinit(TMR_TICK_TIMER);

    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler        = (uint16_t)((g_tmr_clk_hz / TMR_COUNTER_FREQ_HZ) - 1U);
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = (TMR_COUNTER_FREQ_HZ / TMR_TICK_FREQ_HZ) - 1U;
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0U;
    timer_init(TMR_TICK_TIMER, &timer_initpara);

    timer_auto_reload_shadow_enable(TMR_TICK_TIMER);

    /* timer_init() generates an update event to load PSC: drop that flag
       so the first interrupt comes after a full 1 ms period */
    timer_interrupt_flag_clear(TMR_TICK_TIMER, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TMR_TICK_TIMER, TIMER_INT_UP);

    NVIC_SetPriority(TMR_TICK_IRQn, TMR_TICK_IRQ_PRIORITY);
    NVIC_EnableIRQ(TMR_TICK_IRQn);

    timer_enable(TMR_TICK_TIMER);
}

void TMR_Tick_Increment(void)
{
    s_tick_ms++;
}

uint32_t TMR_GetTick(void)
{
    return s_tick_ms;
}

bool TMR_Elapsed(uint32_t *last, uint32_t period_ms)
{
    uint32_t now = s_tick_ms;

    /* unsigned subtraction is wrap-safe (49.7 days) */
    if ((now - *last) >= period_ms) {
        *last = now;
        return true;
    }
    return false;
}

void TMR_DelayMs(uint32_t ms)
{
    uint32_t start = s_tick_ms;

    while ((s_tick_ms - start) < ms) {
    }
}
