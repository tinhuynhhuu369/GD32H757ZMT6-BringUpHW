/*!
    \file    BTN_Driver.c
    \brief   front-panel buttons: polled from the 1 ms TIMER6 tick,
             20 ms debounce, one queued event per press
*/

#include "BTN_Driver.h"

typedef struct {
    uint32_t port;
    uint32_t pin;
    rcu_periph_enum clk;
} btn_hw_t;

static const btn_hw_t s_btn_hw[BTN_COUNT] = {
    [BTN_HOME]   = {BTN_HOME_PORT,   BTN_HOME_PIN,   BTN_HOME_CLK},
    [BTN_RETURN] = {BTN_RETURN_PORT, BTN_RETURN_PIN, BTN_RETURN_CLK},
    [BTN_OK]     = {BTN_OK_PORT,     BTN_OK_PIN,     BTN_OK_CLK},
    [BTN_UP]     = {BTN_UP_PORT,     BTN_UP_PIN,     BTN_UP_CLK},
    [BTN_DOWN]   = {BTN_DOWN_PORT,   BTN_DOWN_PIN,   BTN_DOWN_CLK},
    [BTN_LEFT]   = {BTN_LEFT_PORT,   BTN_LEFT_PIN,   BTN_LEFT_CLK},
    [BTN_RIGHT]  = {BTN_RIGHT_PORT,  BTN_RIGHT_PIN,  BTN_RIGHT_CLK},
};

volatile uint8_t  g_btn_raw      = 0U;
volatile uint8_t  g_btn_state    = 0U;
volatile uint32_t g_btn_overflow = 0U;

/* ISR only: ms the raw level has differed from the debounced state */
static uint8_t s_debounce_cnt[BTN_COUNT];

/* single producer (ISR writes head) / single consumer (main writes tail) */
static volatile uint8_t s_queue[BTN_QUEUE_SIZE];
static volatile uint8_t s_head = 0U;
static volatile uint8_t s_tail = 0U;

/* active LOW: bit = 1 when the button is held down */
static uint8_t BTN_ReadRaw(void)
{
    uint8_t raw = 0U;

    for (uint32_t i = 0U; i < BTN_COUNT; i++) {
        if (RESET == gpio_input_bit_get(s_btn_hw[i].port, s_btn_hw[i].pin)) {
            raw |= (uint8_t)(1U << i);
        }
    }
    return raw;
}

static void BTN_QueuePush(uint8_t id)
{
    uint8_t next = (uint8_t)((s_head + 1U) & (BTN_QUEUE_SIZE - 1U));

    if (next == s_tail) {
        g_btn_overflow++;
        return;
    }
    s_queue[s_head] = id;
    s_head = next;
}

void BTN_Init(void)
{
    for (uint32_t i = 0U; i < BTN_COUNT; i++) {
        rcu_periph_clock_enable(s_btn_hw[i].clk);
        gpio_mode_set(s_btn_hw[i].port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, s_btn_hw[i].pin);
        s_debounce_cnt[i] = 0U;
    }

    /* let the pull-ups charge the lines before the first read */
    for (volatile uint32_t d = 0U; d < 1000U; d++) {
    }

    /* buttons already held at reset count as the current state, not as a press */
    g_btn_raw   = BTN_ReadRaw();
    g_btn_state = g_btn_raw;
    s_head = 0U;
    s_tail = 0U;
}

void BTN_Tick_1ms(void)
{
    uint8_t raw   = BTN_ReadRaw();
    uint8_t state = g_btn_state;

    g_btn_raw = raw;

    for (uint32_t i = 0U; i < BTN_COUNT; i++) {
        uint8_t mask = (uint8_t)(1U << i);

        if ((raw & mask) == (state & mask)) {
            /* level agrees with the debounced state (or bounced back) */
            s_debounce_cnt[i] = 0U;
            continue;
        }
        if (++s_debounce_cnt[i] < BTN_DEBOUNCE_MS) {
            continue;
        }

        /* stable for BTN_DEBOUNCE_MS: accept the new level */
        s_debounce_cnt[i] = 0U;
        state ^= mask;
        if (0U != (state & mask)) {
            BTN_QueuePush((uint8_t)i);      /* released -> pressed */
        }
    }

    g_btn_state = state;
}

bool BTN_GetPress(btn_id_t *id)
{
    uint8_t tail = s_tail;

    if (tail == s_head) {
        return false;
    }
    *id    = (btn_id_t)s_queue[tail];
    s_tail = (uint8_t)((tail + 1U) & (BTN_QUEUE_SIZE - 1U));
    return true;
}

bool BTN_IsPressed(btn_id_t id)
{
    return (0U != (g_btn_state & (uint8_t)(1U << id)));
}
