/*!
    \file    VBUS_Driver.c
    \brief   USB VBUS detect on PG8 (digital input, pull-down)
*/

#include "VBUS_Driver.h"

void VBUS_Init(void)
{
    rcu_periph_clock_enable(VBUS_CLK);
    gpio_mode_set(VBUS_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, VBUS_PIN);
    /* speed only matters for outputs; set to the lowest for consistency */
    gpio_output_options_set(VBUS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, VBUS_PIN);
}

eBool_t HasVBUS(void)
{
    return (SET == gpio_input_bit_get(VBUS_PORT, VBUS_PIN)) ? eTRUE : eFALSE;
}
