/*!
    \file    BUZZER_Driver.c
    \brief   magnetic buzzer on PF13 / TIMER23_CH2 PWM. Beep patterns are
             sequenced by the 1 ms timer tick (no delay, IRQs stay enabled)
*/

#include "BUZZER_Driver.h"
#include "TIMER_Tick_Driver.h"

volatile uint32_t g_buz_freq_hz = 0U;

/* Sequencer, written by main only while s_phase_ms == 0 (ISR idle), then
   s_phase_ms is armed last. The ISR runs it while s_phase_ms != 0.
   0 = no timed sound (silent or continuous BUZ_Tone) */
static volatile uint32_t s_phase_ms  = 0U;   /* time left in the current phase */
static volatile uint8_t  s_beeps_left = 0U;  /* beeps left, current one included */
static volatile bool     s_in_gap    = false;
static uint32_t          s_beep_ms   = 0U;
static uint32_t          s_gap_ms    = 0U;

static volatile bool     s_enabled   = true;
static volatile uint8_t  s_volume    = BUZ_VOLUME_DEFAULT;

static void BUZ_Output_Off(void)
{
    /* force LOW takes effect immediately (no shadow register) */
    timer_channel_output_mode_config(BUZ_TIMER, BUZ_TIMER_CH, TIMER_OC_MODE_LOW);
    g_buz_freq_hz = 0U;
}

/* restart the PWM from the start of a period with the CAR / CV already set */
static void BUZ_Output_On(void)
{
    timer_event_software_generate(BUZ_TIMER, TIMER_EVENT_SRC_UPG);
    timer_channel_output_mode_config(BUZ_TIMER, BUZ_TIMER_CH, TIMER_OC_MODE_PWM0);
}

/* main context: stop the sequencer, then load period / full-volume high
   time (us); the high time is scaled by the volume */
static void BUZ_Start(uint32_t period_us, uint32_t high_us)
{
    uint32_t vol = s_volume;

    s_phase_ms = 0U;    /* ISR idle from here on */

    high_us = (high_us * vol) / 100U;
    if ((0U == high_us) && (0U != vol)) {
        high_us = 1U;   /* lowest audible step */
    }

    timer_autoreload_value_config(BUZ_TIMER, (uint64_t)(period_us - 1U));
    timer_channel_output_pulse_value_config(BUZ_TIMER, BUZ_TIMER_CH, high_us);

    if (0U == g_buz_freq_hz) {
        /* coming from silence: load CAR / CV now and start a fresh period */
        BUZ_Output_On();
    }
    /* else: shadow registers, the new values start at the next period */
    g_buz_freq_hz = BUZ_COUNTER_FREQ_HZ / period_us;
}

/* main context: arm the sequencer (output already playing the first beep) */
static void BUZ_Arm(uint8_t beeps, uint32_t beep_ms, uint32_t gap_ms)
{
    s_beep_ms    = beep_ms;
    s_gap_ms     = gap_ms;
    s_beeps_left = beeps;
    s_in_gap     = false;
    s_phase_ms   = beep_ms;     /* armed last */
}

void BUZ_Init(void)
{
    timer_parameter_struct    timer_initpara;
    timer_oc_parameter_struct oc_initpara;
    uint32_t                  timer_clk = TMR_GetApb1TimerClock();

    /* PF13 -> AF14 (TIMER23_CH2), push-pull, no pull */
    rcu_periph_clock_enable(BUZ_GPIO_CLK);
    gpio_af_set(BUZ_PORT, BUZ_GPIO_AF, BUZ_PIN);
    gpio_mode_set(BUZ_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BUZ_PIN);
    gpio_output_options_set(BUZ_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, BUZ_PIN);

    rcu_periph_clock_enable(BUZ_TIMER_CLK);
    timer_deinit(BUZ_TIMER);

    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler         = (uint16_t)((timer_clk / BUZ_COUNTER_FREQ_HZ) - 1U);
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = BUZ_SDS_PERIOD_US - 1U;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0U;
    timer_init(BUZ_TIMER, &timer_initpara);

    timer_channel_output_struct_para_init(&oc_initpara);
    oc_initpara.outputstate  = TIMER_CCX_ENABLE;
    oc_initpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_channel_output_config(BUZ_TIMER, BUZ_TIMER_CH, &oc_initpara);

    /* CAR / CHxCV shadowed: a new frequency starts at the next period,
       no truncated / glitched pulse */
    timer_auto_reload_shadow_enable(BUZ_TIMER);
    timer_channel_output_shadow_config(BUZ_TIMER, BUZ_TIMER_CH, TIMER_OC_SHADOW_ENABLE);
    timer_channel_output_pulse_value_config(BUZ_TIMER, BUZ_TIMER_CH, 0U);

    /* start silent; the counter runs all the time, only the mode changes */
    BUZ_Output_Off();
    timer_enable(BUZ_TIMER);
}

void BUZ_Play(buz_tone_t tone)
{
    if ((!s_enabled) || (0U == (uint8_t)tone)) {
        return;
    }
    BUZ_Start(BUZ_SDS_PERIOD_US, BUZ_SDS_HIGH_US);
    BUZ_Arm((uint8_t)tone, BUZ_SDS_BEEP_MS, BUZ_SDS_GAP_MS);
}

void BUZ_Enable(bool enable)
{
    s_enabled = enable;
    if (!enable) {
        BUZ_Stop();
    }
}

void BUZ_SetVolume(uint8_t percent)
{
    s_volume = (percent > 100U) ? 100U : percent;
}

uint8_t BUZ_GetVolume(void)
{
    return s_volume;
}

void BUZ_Tone(uint32_t freq_hz)
{
    uint32_t period;

    if (freq_hz < BUZ_FREQ_MIN_HZ) {
        freq_hz = BUZ_FREQ_MIN_HZ;
    } else if (freq_hz > BUZ_FREQ_MAX_HZ) {
        freq_hz = BUZ_FREQ_MAX_HZ;
    }
    period = BUZ_COUNTER_FREQ_HZ / freq_hz;
    BUZ_Start(period, period / 2U);
}

void BUZ_Beep(uint32_t freq_hz, uint32_t duration_ms)
{
    if (0U == duration_ms) {
        return;
    }
    BUZ_Tone(freq_hz);
    BUZ_Arm(1U, duration_ms, 0U);
}

void BUZ_Stop(void)
{
    s_phase_ms = 0U;
    BUZ_Output_Off();
}

bool BUZ_IsBusy(void)
{
    return (0U != g_buz_freq_hz) || (0U != s_phase_ms);
}

void BUZ_Tick_1ms(void)
{
    uint32_t ms = s_phase_ms;

    if (0U == ms) {
        return;
    }
    if (0U != --ms) {
        s_phase_ms = ms;
        return;
    }

    /* current phase finished */
    if (s_in_gap) {
        /* gap over -> next beep (CAR / CV unchanged) */
        s_in_gap   = false;
        BUZ_Output_On();
        g_buz_freq_hz = BUZ_COUNTER_FREQ_HZ / (TIMER_CAR(BUZ_TIMER) + 1U);
        s_phase_ms = s_beep_ms;
        return;
    }

    /* beep over */
    BUZ_Output_Off();
    if (s_beeps_left > 1U) {
        s_beeps_left--;
        s_in_gap   = true;
        /* gap of 0 ms still needs one tick of silence */
        s_phase_ms = (0U != s_gap_ms) ? s_gap_ms : 1U;
    } else {
        s_beeps_left = 0U;
        s_phase_ms   = 0U;
    }
}
