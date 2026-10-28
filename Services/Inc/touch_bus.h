#ifndef TOUCH_BUS_H
#define TOUCH_BUS_H

#include <stdbool.h>
#include <stdint.h>

void touch_bus_init(void);
void touch_bus_select(bool selected);
uint8_t touch_bus_transfer(uint8_t output);
bool touch_bus_pen_is_down(void);

#endif
