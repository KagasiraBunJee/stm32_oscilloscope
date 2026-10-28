#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_CORE_CLOCK_HZ 144000000UL
#define BOARD_USB_SD_CLOCK_HZ 48000000UL

bool board_clock_init(void);
void board_init(void);
void board_delay_ms(uint32_t milliseconds);
uint32_t board_millis(void);
void board_led_set(uint32_t led, bool on);
void board_led_toggle(uint32_t led);

#endif
