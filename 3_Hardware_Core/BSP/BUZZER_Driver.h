#ifndef __BUZZER_DRIVER_H__
#define __BUZZER_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#include "gd32h73x_75x.h"

/* ---------------------------------------------------------------------------
 * Magnetic buzzer (BZ, HSM-DCW12065) on PF13 (LQFP144 pin 53), driven by PWM
 *
 *   PF13  AF14 = TIMER23_CH2  (datasheet table 2-11, Port F AF summary)
 *
 *   CK_TIMER (APB1, same as TIMER6) --PSC--> 1 MHz counter (1 count = 1 us)
 *   CAR = period_us - 1, CH2CV = high_us
 *
 * Silent = channel forced LOW (transistor off, no DC through the coil).
 * BUZ_Tick_1ms() runs in TIMER6_IRQHandler and sequences the beeps, so
 * BUZ_Play() / BUZ_Beep() never block.
 * ------------------------------------------------------------------------- */
#define BUZ_PORT                GPIOF
#define BUZ_PIN                 GPIO_PIN_13
#define BUZ_GPIO_CLK            RCU_GPIOF
#define BUZ_GPIO_AF             GPIO_AF_14

#define BUZ_TIMER               TIMER23
#define BUZ_TIMER_CLK           RCU_TIMER23
#define BUZ_TIMER_CH            TIMER_CH_2

#define BUZ_COUNTER_FREQ_HZ     1000000U    /* counter clock after prescaler */
#define BUZ_FREQ_MIN_HZ         20U
#define BUZ_FREQ_MAX_HZ         20000U

/* ---------------------------------------------------------------------------
 * Beep sound copied from the SDS43 STM32H5 BuzzerDriver (bit-banged there):
 *   TONE_T_ON 110 / TONE_T_OFF 120 / BEEP_TIME 900 / BEEP_DELAY 70
 * Its DelayUsNop() counts TIM13 at 250 MHz / 195 = 1.282 MHz, so one "us"
 * there is really 0.78 us. Converted to real time:
 *   high   110 x 0.78        =  85.8 us
 *   period 230 x 0.78        = 179.4 us  -> ~5.6 kHz, 48 % duty
 *   beep   900 x 179.4 us    = 161.5 ms
 *   gap    70 x 1000 x 0.78  =  54.6 ms  (between beeps of one tone)
 * ------------------------------------------------------------------------- */
#define BUZ_SDS_PERIOD_US       179U
#define BUZ_SDS_HIGH_US         86U
#define BUZ_SDS_BEEP_MS         161U
#define BUZ_SDS_GAP_MS          55U

/* ---------------------------------------------------------------------------
 * Volume = high time scaled down, frequency unchanged (same pitch).
 *   high_us = full_high_us * volume / 100   (100 % = SDS sound, 48 % duty)
 * Loudness of the fundamental ~ sin(pi * duty):
 *   100 % -> 0 dB, 50 % -> -3 dB, 20 % -> -10 dB, 10 % -> -16 dB
 * ------------------------------------------------------------------------- */
#define BUZ_VOLUME_DEFAULT      20U         /* percent, 0 = mute */

/* number of beeps, same values as the SDS enumTone */
typedef enum {
    BUZ_TONE_KEY   = 1,
    BUZ_TONE_POWER = 2,
    BUZ_TONE_DONE  = 4,
    BUZ_TONE_ERASE = 4,
    BUZ_TONE_ERROR = 8
} buz_tone_t;

/* Debug (Live Watch) */
extern volatile uint32_t g_buz_freq_hz;     /* tone currently played, 0 = silent */

void BUZ_Init(void);                        /* GPIO AF + TIMER23 PWM, silent */

/* SDS beep pattern: 'tone' beeps of 161 ms with 55 ms gaps (non-blocking).
   A new call replaces the pattern that is playing. */
void BUZ_Play(buz_tone_t tone);
void BUZ_Enable(bool enable);               /* false = BUZ_Play() is muted */

/* 0..100 %, applied from the next sound started */
void    BUZ_SetVolume(uint8_t percent);
uint8_t BUZ_GetVolume(void);

/* free tone at 50 % duty: continuous until BUZ_Stop() / timed (non-blocking) */
void BUZ_Tone(uint32_t freq_hz);
void BUZ_Beep(uint32_t freq_hz, uint32_t duration_ms);

void BUZ_Stop(void);
bool BUZ_IsBusy(void);                      /* true while a sound is playing */

/* called from TIMER6_IRQHandler every 1 ms */
void BUZ_Tick_1ms(void);

#endif /* __BUZZER_DRIVER_H__ */
