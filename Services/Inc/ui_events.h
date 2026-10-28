#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_EVENT_CAPTURE_READY = 0,
    UI_EVENT_STORAGE_CHANGED,
    UI_EVENT_ACQUISITION_ERROR
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    uint32_t value;
} ui_event_t;

bool ui_post_event(ui_event_type_t type, uint32_t value);
bool ui_get_event(ui_event_t *event);

#endif
