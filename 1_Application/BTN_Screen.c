/*!
    \file    BTN_Screen.c
    \brief   one full-screen page per button, drawn with the LCD fill / line
             primitives only (no font yet)

    Screen 800 x 480, centre (400, 240).
        Home    BLUE    / WHITE   house
        Return  MAGENTA / WHITE   U-turn arrow pointing left
        OK      GREEN   / WHITE   large square
        Up      CYAN    / BLACK   arrow up
        Down    YELLOW  / BLACK   arrow down
        Left    RED     / WHITE   arrow left
        Right   BLACK   / WHITE   arrow right

    Each press plays the SDS key tone (BUZ_TONE_KEY, one beep).
*/

#include "BTN_Screen.h"
#include "LCD_ILI9806G_Driver.h"
#include "LED_Driver.h"
#include "BUZZER_Driver.h"

#define SCR_CX      (ILI9806G_WIDTH / 2U)       /* 400 */
#define SCR_CY      (ILI9806G_HEIGHT / 2U)      /* 240 */

typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} dir_t;

typedef struct {
    uint16_t bg;
    uint16_t fg;
} page_color_t;

static const page_color_t s_page_color[BTN_COUNT] = {
    [BTN_HOME]   = {ILI9806G_BLUE,    ILI9806G_WHITE},
    [BTN_RETURN] = {ILI9806G_MAGENTA, ILI9806G_WHITE},
    [BTN_OK]     = {ILI9806G_GREEN,   ILI9806G_WHITE},
    [BTN_UP]     = {ILI9806G_CYAN,    ILI9806G_BLACK},
    [BTN_DOWN]   = {ILI9806G_YELLOW,  ILI9806G_BLACK},
    [BTN_LEFT]   = {ILI9806G_RED,     ILI9806G_WHITE},
    [BTN_RIGHT]  = {ILI9806G_BLACK,   ILI9806G_WHITE},
};

static bool     s_active = false;
static btn_id_t s_last   = BTN_COUNT;   /* last page drawn, BTN_COUNT = none */

/* ---------------------------------------------------------------------------
 * Filled isosceles triangle, apex at (ax, ay), pointing towards dir.
 * h = height in pixels, base = 2h - 1. Drawn line by line.
 * ------------------------------------------------------------------------- */
static void draw_triangle(uint16_t ax, uint16_t ay, uint16_t h, dir_t dir, uint16_t color)
{
    for (uint16_t i = 0U; i < h; i++) {
        uint16_t len = (uint16_t)(2U * i + 1U);

        switch (dir) {
        case DIR_UP:    ILI9806G_LCD_DrawHLine(ax - i, ay + i, len, color); break;
        case DIR_DOWN:  ILI9806G_LCD_DrawHLine(ax - i, ay - i, len, color); break;
        case DIR_LEFT:  ILI9806G_LCD_DrawVLine(ax + i, ay - i, len, color); break;
        case DIR_RIGHT: ILI9806G_LCD_DrawVLine(ax - i, ay - i, len, color); break;
        default: break;
        }
    }
}

/* arrow = triangle head (h = 200) + 100 px wide stem */
static void draw_arrow(dir_t dir, uint16_t color)
{
    switch (dir) {
    case DIR_UP:
        draw_triangle(SCR_CX, 90U, 200U, DIR_UP, color);            /* y 90..289  */
        ILI9806G_LCD_Fill_Rect(350U, 290U, 450U, 390U, color);
        break;
    case DIR_DOWN:
        draw_triangle(SCR_CX, 390U, 200U, DIR_DOWN, color);         /* y 191..390 */
        ILI9806G_LCD_Fill_Rect(350U, 90U, 450U, 190U, color);
        break;
    case DIR_LEFT:
        draw_triangle(200U, SCR_CY, 200U, DIR_LEFT, color);         /* x 200..399 */
        ILI9806G_LCD_Fill_Rect(400U, 190U, 600U, 290U, color);
        break;
    case DIR_RIGHT:
        draw_triangle(600U, SCR_CY, 200U, DIR_RIGHT, color);        /* x 401..600 */
        ILI9806G_LCD_Fill_Rect(200U, 190U, 400U, 290U, color);
        break;
    default:
        break;
    }
}

static void draw_home(uint16_t fg, uint16_t bg)
{
    draw_triangle(SCR_CX, 60U, 150U, DIR_UP, fg);                   /* roof y 60..209 */
    ILI9806G_LCD_Fill_Rect(290U, 210U, 510U, 410U, fg);             /* walls */
    ILI9806G_LCD_Fill_Rect(370U, 300U, 430U, 410U, bg);             /* door */
}

static void draw_return(uint16_t fg)
{
    /* head on the top bar pointing left, then right / down / back left */
    draw_triangle(180U, 165U, 100U, DIR_LEFT, fg);                  /* x 180..279 */
    ILI9806G_LCD_Fill_Rect(280U, 140U, 560U, 190U, fg);             /* top bar */
    ILI9806G_LCD_Fill_Rect(510U, 140U, 560U, 360U, fg);             /* right bar */
    ILI9806G_LCD_Fill_Rect(300U, 310U, 560U, 360U, fg);             /* bottom bar */
}

static void draw_ok(uint16_t fg)
{
    ILI9806G_LCD_Fill_Rect(275U, 115U, 525U, 365U, fg);
}

void BTN_Screen_Draw(btn_id_t id)
{
    uint16_t bg;
    uint16_t fg;

    if (id >= BTN_COUNT) {
        return;
    }
    s_last = id;
    bg = s_page_color[id].bg;
    fg = s_page_color[id].fg;

    ILI9806G_LCD_Fill_Screen(bg);

    switch (id) {
    case BTN_HOME:   draw_home(fg, bg);          break;
    case BTN_RETURN: draw_return(fg);            break;
    case BTN_OK:     draw_ok(fg);                break;
    case BTN_UP:     draw_arrow(DIR_UP, fg);     break;
    case BTN_DOWN:   draw_arrow(DIR_DOWN, fg);   break;
    case BTN_LEFT:   draw_arrow(DIR_LEFT, fg);   break;
    case BTN_RIGHT:  draw_arrow(DIR_RIGHT, fg);  break;
    default:         break;
    }
}

void BTN_Screen_Redraw(void)
{
    BTN_Screen_Draw(s_last);    /* no-op while s_last == BTN_COUNT */
}

void BTN_Screen_Task(void)
{
    btn_id_t id;
    btn_id_t last = BTN_COUNT;

    /* a full-screen fill takes tens of ms: only the newest press is drawn */
    while (BTN_GetPress(&id)) {
        last = id;
    }
    if (BTN_COUNT == last) {
        return;
    }

    s_active = true;
    /* beep first: non-blocking, it plays while the page is drawn */
    BUZ_Play(BUZ_TONE_KEY);
    BTN_Screen_Draw(last);
    /* YELLOW LED toggles once per handled press */
    LED_Toggle(LED_YELLOW);
}

bool BTN_Screen_IsActive(void)
{
    return s_active;
}
