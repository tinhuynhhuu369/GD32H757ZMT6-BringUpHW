/*!
    \file    VBUS_Screen.c
    \brief   VBUS test: USB plugged -> USB icon page, unplugged -> previous
             screen. Drawn with the LCD fill / line primitives only.

    Screen 800 x 480, USB "trident" centred on x = 400:
        arrow head   apex (400, 40), 70 px high
        shaft        x 390..409, y 110..380
        bottom dot   circle (400, 395), r 40
        left arm     45 deg from (400, 330) to (300, 231), up to circle (300, 170)
        right arm    45 deg from (400, 270) to (500, 171), up to square 475..525
*/

#include "VBUS_Screen.h"
#include "VBUS_Driver.h"
#include "BTN_Screen.h"
#include "LCD_ILI9806G_Driver.h"
#include "TIMER_Tick_Driver.h"

#define USB_BG          0x18C3U         /* dark grey (RGB565), unused elsewhere */
#define USB_FG          ILI9806G_WHITE

#define USB_CX          400U
#define USB_LINE_W      20U             /* shaft / vertical arm width */
#define USB_DIAG_W      28U             /* row width of a 45 deg arm (~20 px thick) */

static bool     s_shown    = false;     /* USB page currently on the LCD */
static eBool_t  s_vbus     = eFALSE;    /* debounced VBUS level */
static uint32_t s_change_t0 = 0U;       /* tick when the raw level first differed */
static bool     s_changing = false;

static uint16_t isqrt_u32(uint32_t v)
{
    uint32_t r = 0U;

    while ((r + 1U) * (r + 1U) <= v) {
        r++;
    }
    return (uint16_t)r;
}

static void draw_circle_filled(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color)
{
    for (int32_t dy = -(int32_t)r; dy <= (int32_t)r; dy++) {
        uint16_t half = isqrt_u32((uint32_t)r * r - (uint32_t)(dy * dy));

        ILI9806G_LCD_DrawHLine(cx - half, (uint16_t)((int32_t)cy + dy),
                               (uint16_t)(2U * half + 1U), color);
    }
}

/* triangle pointing up, apex (ax, ay), h rows, base 2h - 1 */
static void draw_triangle_up(uint16_t ax, uint16_t ay, uint16_t h, uint16_t color)
{
    for (uint16_t i = 0U; i < h; i++) {
        ILI9806G_LCD_DrawHLine(ax - i, ay + i, (uint16_t)(2U * i + 1U), color);
    }
}

/* 45 deg arm going up from centre (x, y) for 'rows' rows, dir = -1 left / +1 right */
static void draw_diag_up(uint16_t x, uint16_t y, uint16_t rows, int32_t dir, uint16_t color)
{
    for (uint16_t i = 0U; i < rows; i++) {
        uint16_t xc = (uint16_t)((int32_t)x + dir * (int32_t)i);

        ILI9806G_LCD_DrawHLine(xc - USB_DIAG_W / 2U, y - i, USB_DIAG_W, color);
    }
}

static void VBUS_Screen_Draw(void)
{
    ILI9806G_LCD_Fill_Screen(USB_BG);

    /* arrow head + shaft + bottom dot */
    draw_triangle_up(USB_CX, 40U, 70U, USB_FG);
    ILI9806G_LCD_Fill_Rect(USB_CX - USB_LINE_W / 2U, 110U,
                           USB_CX + USB_LINE_W / 2U - 1U, 380U, USB_FG);
    draw_circle_filled(USB_CX, 395U, 40U, USB_FG);

    /* left arm -> circle */
    draw_diag_up(USB_CX, 330U, 100U, -1, USB_FG);
    ILI9806G_LCD_Fill_Rect(290U, 180U, 309U, 231U, USB_FG);
    draw_circle_filled(300U, 170U, 28U, USB_FG);

    /* right arm -> square */
    draw_diag_up(USB_CX, 270U, 100U, +1, USB_FG);
    ILI9806G_LCD_Fill_Rect(490U, 140U, 509U, 171U, USB_FG);
    ILI9806G_LCD_Fill_Rect(475U, 95U, 525U, 145U, USB_FG);
}

/* put back what was on the LCD before the USB page */
static void VBUS_Screen_Restore(void)
{
    if (BTN_Screen_IsActive()) {
        BTN_Screen_Redraw();            /* last button page */
    } else {
        ILI9806G_LCD_Test_Reset();      /* colour test redraws from its first step */
    }
}

/* debounced VBUS level: a change is accepted after VBUS_SCREEN_DEBOUNCE_MS */
static eBool_t VBUS_Screen_ReadDebounced(void)
{
    eBool_t raw = HasVBUS();

    if (raw == s_vbus) {
        s_changing = false;
    } else if (!s_changing) {
        s_changing  = true;
        s_change_t0 = TMR_GetTick();
    } else if ((TMR_GetTick() - s_change_t0) >= VBUS_SCREEN_DEBOUNCE_MS) {
        s_changing = false;
        s_vbus     = raw;
    }
    return s_vbus;
}

void VBUS_Screen_Task(void)
{
    eBool_t vbus = VBUS_Screen_ReadDebounced();

    if ((eTRUE == vbus) && !s_shown) {
        s_shown = true;
        VBUS_Screen_Draw();
    } else if ((eFALSE == vbus) && s_shown) {
        s_shown = false;
        VBUS_Screen_Restore();
    }

    if (s_shown) {
        /* presses while the USB page is up are dropped, so the rollback
           shows the old screen and not a page queued meanwhile */
        btn_id_t id;
        while (BTN_GetPress(&id)) {
        }
    }
}

bool VBUS_Screen_IsShown(void)
{
    return s_shown;
}
