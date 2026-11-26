#ifndef __BTN_SCREEN_H__
#define __BTN_SCREEN_H__

#include <stdbool.h>

#include "BTN_Driver.h"

/* draw the full-screen page of one button (background colour + icon) */
void BTN_Screen_Draw(btn_id_t id);

/* main loop: takes pending presses and draws the page of the last one */
void BTN_Screen_Task(void);

/* true once the first press has been handled (the LCD then belongs to
   BTN_Screen and the LCD colour test must stop) */
bool BTN_Screen_IsActive(void);

#endif /* __BTN_SCREEN_H__ */
