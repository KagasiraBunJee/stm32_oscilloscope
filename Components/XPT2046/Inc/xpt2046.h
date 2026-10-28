#ifndef XPT2046_H
#define XPT2046_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
} xpt2046_point_t;

void xpt2046_init(void);
bool xpt2046_is_pressed(void);
bool xpt2046_read_raw(xpt2046_point_t *point);
bool xpt2046_read_pixel(xpt2046_point_t *point);

#endif
