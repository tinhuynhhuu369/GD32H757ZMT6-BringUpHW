#ifndef __VBUS_SCREEN_H__
#define __VBUS_SCREEN_H__

#include <stdbool.h>

/* VBUS (PG8) must stay at the same level this long before it is accepted */
#define VBUS_SCREEN_DEBOUNCE_MS     50U

/* main loop:
 *   USB plugged   -> full-screen USB icon page
 *   USB unplugged -> roll back to the previous screen
 *                    (last button page, or the LCD colour test restarts) */
void VBUS_Screen_Task(void);

/* true while the USB page owns the LCD: the caller must not draw */
bool VBUS_Screen_IsShown(void);

#endif /* __VBUS_SCREEN_H__ */
