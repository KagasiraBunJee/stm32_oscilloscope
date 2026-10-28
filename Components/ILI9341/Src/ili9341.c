#include "ili9341.h"

#include <stddef.h>

#include "lcd_bus.h"

/*
 * MX + MV selects the MRB3205's non-reflected 320x240 scan direction; BGR
 * preserves the panel colour order.  Adding MY here mirrors the image along
 * the landscape X axis on this panel.
 */
#define ILI9341_MADCTL_LANDSCAPE 0x68U

static void lcd_write_command(uint8_t command)
{
    lcd_bus_write_command(command);
}

static void lcd_write_data(uint8_t data)
{
    lcd_bus_write_data8(data);
}

static void lcd_write_u16(uint16_t data)
{
    lcd_bus_write_data16(data);
}

static void lcd_command_data(uint8_t command, const uint8_t *data,
                             uint32_t length)
{
    lcd_write_command(command);
    while (length-- != 0UL) {
        lcd_write_data(*data++);
    }
}

static void ili9341_set_window(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1)
{
    lcd_write_command(0x2A);
    lcd_write_data((uint8_t)(x0 >> 8));
    lcd_write_data((uint8_t)x0);
    lcd_write_data((uint8_t)(x1 >> 8));
    lcd_write_data((uint8_t)x1);

    lcd_write_command(0x2B);
    lcd_write_data((uint8_t)(y0 >> 8));
    lcd_write_data((uint8_t)y0);
    lcd_write_data((uint8_t)(y1 >> 8));
    lcd_write_data((uint8_t)y1);
    lcd_write_command(0x2C);
}

void ili9341_init(void)
{
    /* Exact ILI9341 sequence supplied for the LCDWiki MRB3205 module. */
    static const uint8_t power_b[] = {0x00, 0xD9, 0x30};
    static const uint8_t power_seq[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t timing_a[] = {0x85, 0x10, 0x7A};
    static const uint8_t power_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    static const uint8_t timing_b[] = {0x00, 0x00};
    static const uint8_t vcom[] = {0x26, 0x26};
    static const uint8_t display_function[] = {0x0A, 0xA2};
    static const uint8_t gamma_pos[] = {
        0x1F, 0x24, 0x24, 0x0D, 0x12, 0x09, 0x52, 0xB7,
        0x3F, 0x0C, 0x15, 0x06, 0x0E, 0x08, 0x00
    };
    static const uint8_t gamma_neg[] = {
        0x00, 0x1B, 0x1B, 0x02, 0x0E, 0x06, 0x2E, 0x48,
        0x3F, 0x03, 0x0A, 0x09, 0x31, 0x37, 0x1F
    };

    lcd_bus_init();
    lcd_bus_delay_ms(100);

    /* LCD RST is held high externally, so reset the controller in software. */
    lcd_write_command(0x01);
    lcd_bus_delay_ms(150);

    lcd_command_data(0xCF, power_b, sizeof(power_b));
    lcd_command_data(0xED, power_seq, sizeof(power_seq));
    lcd_command_data(0xE8, timing_a, sizeof(timing_a));
    lcd_command_data(0xCB, power_a, sizeof(power_a));
    lcd_command_data(0xEA, timing_b, sizeof(timing_b));
    lcd_write_command(0xF7); lcd_write_data(0x20);
    lcd_write_command(0xC0); lcd_write_data(0x1B);
    lcd_write_command(0xC1); lcd_write_data(0x12);
    lcd_command_data(0xC5, vcom, sizeof(vcom));
    lcd_write_command(0xC7); lcd_write_data(0xB0);
    lcd_write_command(0x36);
    lcd_write_data(ILI9341_MADCTL_LANDSCAPE);
    lcd_write_command(0x3A); lcd_write_data(0x55); /* RGB565. */
    lcd_write_command(0xB1); lcd_write_data(0x00); lcd_write_data(0x1A);
    lcd_command_data(0xB6, display_function, sizeof(display_function));
    lcd_write_command(0xF2); lcd_write_data(0x00);
    lcd_write_command(0x26); lcd_write_data(0x01);
    lcd_command_data(0xE0, gamma_pos, sizeof(gamma_pos));
    lcd_command_data(0xE1, gamma_neg, sizeof(gamma_neg));
    lcd_write_command(0x2B);
    lcd_write_data(0x00); lcd_write_data(0x00);
    lcd_write_data(0x01); lcd_write_data(0x3F);
    lcd_write_command(0x2A);
    lcd_write_data(0x00); lcd_write_data(0x00);
    lcd_write_data(0x00); lcd_write_data(0xEF);
    lcd_write_command(0x11);
    lcd_bus_delay_ms(120);
    lcd_write_command(0x29);
    lcd_bus_delay_ms(20);
    ili9341_set_backlight(1);

}

void ili9341_set_backlight(uint8_t on)
{
    lcd_bus_set_backlight(on != 0U);
}

void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color)
{
    uint32_t count;

    if ((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT) ||
        (width == 0U) || (height == 0U)) {
        return;
    }
    if ((uint32_t)x + width > ILI9341_WIDTH) {
        width = (uint16_t)(ILI9341_WIDTH - x);
    }
    if ((uint32_t)y + height > ILI9341_HEIGHT) {
        height = (uint16_t)(ILI9341_HEIGHT - y);
    }

    ili9341_set_window(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U));
    count = (uint32_t)width * height;
    while (count-- != 0UL) {
        lcd_write_u16(color);
    }
}

void ili9341_fill(uint16_t color)
{
    ili9341_fill_rect(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) {
        return;
    }
    ili9341_set_window(x, y, x, y);
    lcd_write_u16(color);
}

void ili9341_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       uint16_t color)
{
    const int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    const int32_t sx = (x0 < x1) ? 1 : -1;
    const int32_t dy_abs = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    const int32_t dy = -dy_abs;
    const int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t error = dx + dy;

    for (;;) {
        if ((x0 >= 0) && (x0 < (int32_t)ILI9341_WIDTH) &&
            (y0 >= 0) && (y0 < (int32_t)ILI9341_HEIGHT)) {
            ili9341_draw_pixel((uint16_t)x0, (uint16_t)y0, color);
        }
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }
        {
            const int32_t twice_error = 2 * error;
            if (twice_error >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice_error <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
}

void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color)
{
    if ((width == 0U) || (height == 0U)) {
        return;
    }
    ili9341_fill_rect(x, y, width, 1, color);
    ili9341_fill_rect(x, (uint16_t)(y + height - 1U), width, 1, color);
    ili9341_fill_rect(x, y, 1, height, color);
    ili9341_fill_rect((uint16_t)(x + width - 1U), y, 1, height, color);
}

bool ili9341_draw_rgb565_dma(uint16_t x, uint16_t y, uint16_t width,
                             uint16_t height, const uint16_t *pixels)
{
    if ((pixels == 0) || (width == 0U) || (height == 0U) ||
        (x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT) ||
        ((uint32_t)x + width > ILI9341_WIDTH) ||
        ((uint32_t)y + height > ILI9341_HEIGHT)) {
        return false;
    }

    ili9341_set_window(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U));
    return lcd_bus_write_pixels_dma(pixels, (uint32_t)width * height);
}

bool ili9341_draw_rgb565_dma_async(uint16_t x, uint16_t y, uint16_t width,
                                   uint16_t height, const uint16_t *pixels,
                                   lcd_bus_dma_callback_t callback,
                                   void *context)
{
    if ((pixels == NULL) || (width == 0U) || (height == 0U) ||
        (x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT) ||
        ((uint32_t)x + width > ILI9341_WIDTH) ||
        ((uint32_t)y + height > ILI9341_HEIGHT)) {
        return false;
    }

    ili9341_set_window(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U));
    return lcd_bus_write_pixels_dma_async(pixels, (uint32_t)width * height,
                                          callback, context);
}
