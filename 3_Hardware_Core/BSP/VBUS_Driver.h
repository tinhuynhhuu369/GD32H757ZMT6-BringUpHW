#ifndef __VBUS_DRIVER_H__
#define __VBUS_DRIVER_H__

#include <stdint.h>

#include "gd32h73x_75x.h"

/* ---------------------------------------------------------------------------
 * USB VBUS detect on PG8, digital input, internal pull-down
 *
 *   PG8 HIGH -> USB 5V present
 *   PG8 LOW  -> no USB (pull-down holds the pin low when the cable is out)
 * ------------------------------------------------------------------------- */
#define VBUS_PIN            (GPIO_PIN_8)
#define VBUS_PORT           (GPIOG)
#define VBUS_CLK            (RCU_GPIOG)

#ifndef EBOOL_DEFINED
#define EBOOL_DEFINED
typedef enum {
    eFALSE = 0,
    eTRUE  = 1
} eBool_t;
#endif

void    VBUS_Init(void);    /* GPIO input + pull-down, low speed */
eBool_t HasVBUS(void);      /* eTRUE = USB 5V present (PG8 HIGH) */

#endif /* __VBUS_DRIVER_H__ */
