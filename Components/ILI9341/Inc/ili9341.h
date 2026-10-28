#ifndef ILI9341_H
#define ILI9341_H

#include <stdbool.h>
#include <stdint.h>

#include "lcd_bus.h"

#define ILI9341_WIDTH  320U
#define ILI9341_HEIGHT 240U

#define RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
                (((uint16_t)(g) & 0xFCU) << 3) | \
                ((uint16_t)(b) >> 3)))

void ili9341_init(void);
void ili9341_set_backlight(uint8_t on);
void ili9341_fill(uint16_t color);
void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color);
void ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void ili9341_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       uint16_t color);
void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color);
bool ili9341_draw_rgb565_dma(uint16_t x, uint16_t y, uint16_t width,
                             uint16_t height, const uint16_t *pixels);
bool ili9341_draw_rgb565_dma_async(uint16_t x, uint16_t y, uint16_t width,
                                   uint16_t height, const uint16_t *pixels,
                                   lcd_bus_dma_callback_t callback,
                                   void *context);

#endif
