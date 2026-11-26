/*!
    \file    main.c
    \brief   running LED

    \version 2026-03-24, V1.6.0, firmware for GD32H73x_75x
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32h73x_75x.h"
#include "systick.h"
#include "LCD_ILI9806G_Driver.h"
#include "TIMER_Tick_Driver.h"
#include "LED_Driver.h"
#include "BTN_Driver.h"
#include "BUZZER_Driver.h"
#include "BTN_Screen.h"

/*!
    \brief      configure the MPU: map the LCD EXMC window as Device memory
                (non-cacheable, ordered) so command/data writes are not cached,
                merged or reordered while the D-Cache is enabled
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void mpu_config(void)
{
    mpu_region_init_struct mpu_init_struct;

    mpu_region_struct_para_init(&mpu_init_struct);

    /* disable the MPU while it is being configured */
    ARM_MPU_Disable();
    ARM_MPU_SetRegion(0U, 0U);

    /* region 0: EXMC bank0 region2 (LCD), 64MB, Device memory (TEX=0, C=0, B=1), XN */
    mpu_init_struct.region_number       = MPU_REGION_NUMBER0;
    mpu_init_struct.region_base_address = LCD_EXMC_BANK_BASE;
    mpu_init_struct.region_size         = MPU_REGION_SIZE_64MB;
    mpu_init_struct.access_permission   = MPU_AP_FULL_ACCESS;
    mpu_init_struct.tex_type            = MPU_TEX_TYPE0;
    mpu_init_struct.access_shareable    = MPU_ACCESS_NON_SHAREABLE;
    mpu_init_struct.access_cacheable    = MPU_ACCESS_NON_CACHEABLE;
    mpu_init_struct.access_bufferable   = MPU_ACCESS_BUFFERABLE;
    mpu_init_struct.subregion_disable   = 0x00U;
    mpu_init_struct.instruction_exec    = MPU_INSTRUCTION_EXEC_NOT_PERMIT;
    mpu_region_config(&mpu_init_struct);
    mpu_region_enable();

    /* enable the MPU, default memory map as background for privileged access */
    ARM_MPU_Enable(MPU_MODE_PRIV_DEFAULT);
}

/*!
    \brief      enable the CPU Chache
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void cache_enable(void)
{
    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    SCB_EnableDCache();
}

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    bool lcd_ok;

    /* configure the MPU (LCD region) before enabling the D-Cache */
    mpu_config();
    /* enable the CPU Cache */
    cache_enable();
    /* configure systick */
    systick_config();

    /* LEDs: GPIO output, all off */
    LED_Init();
    /* buttons: GPIO input + pull-up; before TMR_Init so the TIMER6 ISR never
       scans them half-initialised */
    BTN_Init();
    /* buzzer: PF13 = TIMER23_CH2 PWM, silent (timed beeps end in the TIMER6 ISR) */
    BUZ_Init();
    /* TIMER6: 1 ms tick, drives LED blinking and LCD step timing */
    TMR_Init();

    /* GREEN LED = heartbeat, toggled by the TIMER6 ISR (keeps blinking even
       while the main loop is busy filling the LCD) */
    LED_Blink(LED_GREEN, 500U);

    /* LCD: GPIO + EXMC + reset + ID check + init sequence
       (the reset / sleep-out waits use TMR_DelayMs, so TMR_Init must run first) */
    lcd_ok = ILI9806G_LCD_Init();
    /* RED LED on = LCD ID read back failed (see g_lcd_id in the debugger) */
    if(!lcd_ok) {
        LED_On(LED_RED);
    }
    /* backlight on after the panel has been filled */
    ILI9806G_LCD_SetBrightness(1U);
    /* power-on: 2 beeps, same sound as the SDS tools (non-blocking) */
    BUZ_Play(BUZ_TONE_POWER);

    while(1) {
        /* LCD colour test runs only until the first button press,
           after that the LCD belongs to BTN_Screen */
        if(!BTN_Screen_IsActive()) {
            /* non-blocking: draws a step, then returns while it is held */
            ILI9806G_LCD_Test_Task();

            /* YELLOW LED on while the LCD test sequence runs, off during the pause */
            if(ILI9806G_LCD_Test_IsRunning()) {
                LED_On(LED_YELLOW);
            } else {
                LED_Off(LED_YELLOW);
            }
        }

        /* button press -> beep + page of that button (YELLOW LED toggles per press) */
        BTN_Screen_Task();

        /* other non-blocking tasks go here */
    }
}
