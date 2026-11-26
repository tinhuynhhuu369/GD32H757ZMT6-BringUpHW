/*!
    \file    FSMC_LCD_Driver.h
    \brief   EXMC (FSMC) low level access for 16-bit 8080 LCD on GD32H757ZMT6

    Pin map (LQFP144):
        BackLight   pin 19   PF7    GPIO output (HIGH = on)
        LcdRES      pin 117  PD3    GPIO output (active LOW)
        FSMC_NE3    pin 125  PG10   EXMC_NE2  (AF12) -> bank0 region2 @ 0x68000000
        FSMC_A0     pin 10   PF0    EXMC_A0   (AF12) -> RS/DCX
        FSMC_NWE    pin 119  PD5    EXMC_NWE  (AF12)
        FSMC_NOE    pin 118  PD4    EXMC_NOE  (AF12)
        FSMC_D0..1  pin 85,86        PD14, PD15
        FSMC_D2..3  pin 114,115      PD0,  PD1
        FSMC_D4..12 pin 58..68       PE7 .. PE15
        FSMC_D13..15 pin 77,78,79    PD8,  PD9, PD10
*/

#ifndef FSMC_LCD_DRIVER_H
#define FSMC_LCD_DRIVER_H

#include <stdint.h>
#include "gd32h73x_75x.h"

/* bus width: 16-bit 8080 */
#define LCD_USING_16BIT                 (1)

/* ---------------------------------------------------------------------------
 * EXMC region / address
 * ------------------------------------------------------------------------- */
#define LCD_EXMC_REGION                 EXMC_BANK0_NORSRAM_REGION2
#define LCD_EXMC_BANK_BASE              (0x68000000UL)  /* bank0 region2 (NE2) */

/* 16-bit bus: EXMC_A0 is driven by HADDR[1] */
#define LCD_FSMC_ADDRESS_CMD            (LCD_EXMC_BANK_BASE)                /* A0 = 0 */
#define LCD_FSMC_ADDRESS_DATA           (LCD_EXMC_BANK_BASE | (1UL << 1))   /* A0 = 1 */

#define LCD_CMD_REG                     (*(volatile uint16_t *)LCD_FSMC_ADDRESS_CMD)
#define LCD_DATA_REG                    (*(volatile uint16_t *)LCD_FSMC_ADDRESS_DATA)

/* ---------------------------------------------------------------------------
 * Control pins
 * ------------------------------------------------------------------------- */
#define LCD_DCX_PORT                    GPIOF           /* EXMC_A0  */
#define LCD_DCX_PIN                     GPIO_PIN_0
#define LCDCS_PORT                      GPIOG           /* EXMC_NE2 */
#define LCDCS_PIN                       GPIO_PIN_10
#define LCD_WRX_PORT                    GPIOD           /* EXMC_NWE */
#define LCD_WRX_PIN                     GPIO_PIN_5
#define LCDRD_PORT                      GPIOD           /* EXMC_NOE */
#define LCDRD_PIN                       GPIO_PIN_4

#define LCDRES_PORT                     GPIOD           /* GPIO, active LOW */
#define LCDRES_PIN                      GPIO_PIN_3
#define LCDRES_CLK                      RCU_GPIOD

#define LCD_BACKLIGHT_PORT              GPIOF           /* GPIO */
#define LCD_BACKLIGHT_PIN               GPIO_PIN_7
#define LCD_BACKLIGHT_CLK               RCU_GPIOF
/* level that turns the backlight ON: 1 = HIGH, 0 = LOW */
#define LCD_BACKLIGHT_ON_LEVEL          (1)

/* ---------------------------------------------------------------------------
 * Data bus pins (all AF12)
 * ------------------------------------------------------------------------- */
#define LCD_DATA_GPIOD_PINS             (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | \
                                         GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15)
#define LCD_DATA_GPIOE_PINS             (GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | \
                                         GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | \
                                         GPIO_PIN_15)

/* ---------------------------------------------------------------------------
 * Bus access
 * The EXMC window must be mapped as Device memory by the MPU (see main.c),
 * otherwise D-Cache / write merging breaks the command/data ordering.
 * ------------------------------------------------------------------------- */
static inline void NW_LCD_Write_Cmd(uint16_t cmd)
{
    LCD_CMD_REG = cmd;
}

static inline void NW_LCD_Write_Data(uint16_t data)
{
    LCD_DATA_REG = data;
}

static inline uint16_t NW_LCD_Read_Data(void)
{
    return LCD_DATA_REG;
}

#endif /* FSMC_LCD_DRIVER_H */
