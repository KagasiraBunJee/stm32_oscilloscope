#include "ui_events.h"

#include <stddef.h>

#include "stm32f4xx.h"

#define UI_EVENT_QUEUE_LENGTH 8U

static ui_event_t g_queue[UI_EVENT_QUEUE_LENGTH];
static volatile uint8_t g_head;
static volatile uint8_t g_tail;

bool ui_post_event(ui_event_type_t type, uint32_t value)
{
    uint32_t primask = __get_PRIMASK();
    uint8_t next;
    __disable_irq();
    next = (uint8_t)((g_head + 1U) % UI_EVENT_QUEUE_LENGTH);
    if (next == g_tail) {
        if (primask == 0U) __enable_irq();
        return false;
    }
    g_queue[g_head].type = type;
    g_queue[g_head].value = value;
    g_head = next;
    if (primask == 0U) __enable_irq();
    return true;
}

bool ui_get_event(ui_event_t *event)
{
    uint32_t primask;
    if (event == NULL) return false;
    primask = __get_PRIMASK();
    __disable_irq();
    if (g_tail == g_head) {
        if (primask == 0U) __enable_irq();
        return false;
    }
    *event = g_queue[g_tail];
    g_tail = (uint8_t)((g_tail + 1U) % UI_EVENT_QUEUE_LENGTH);
    if (primask == 0U) __enable_irq();
    return true;
}
