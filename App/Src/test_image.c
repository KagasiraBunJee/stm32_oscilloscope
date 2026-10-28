#include "test_image.h"

#include "ili9341.h"

static uint16_t g_scanline[ILI9341_WIDTH] __attribute__((aligned(4)));
volatile uint32_t test_image_frames_completed;

static uint16_t triangle_wave_y(uint16_t x, uint32_t phase)
{
    const uint32_t position = ((uint32_t)x + phase) % 160UL;
    const uint32_t ramp = (position < 80UL) ? position : (159UL - position);

    return (uint16_t)(50UL + ((ramp * 140UL) / 79UL));
}

bool test_image_draw_dma(uint32_t frame)
{
    const uint32_t animation_step = frame >> 2;
    const uint16_t cursor_x = (uint16_t)(animation_step % ILI9341_WIDTH);
    const uint32_t phase = animation_step % 160UL;
    uint16_t y;

    for (y = 0; y < ILI9341_HEIGHT; ++y) {
        uint16_t x;

        for (x = 0; x < ILI9341_WIDTH; ++x) {
            const uint16_t wave_y = triangle_wave_y(x, phase);
            const int32_t wave_delta = (int32_t)y - (int32_t)wave_y;
            uint16_t color = RGB565(0, 6, 14);

            if (((x % 40U) == 0U) || ((y % 30U) == 0U)) {
                color = RGB565(18, 34, 42);
            }
            if ((y >= 119U) && (y <= 120U)) {
                color = RGB565(40, 70, 78);
            }
            if ((wave_delta >= -2) && (wave_delta <= 2)) {
                color = RGB565(0, 255, 80);
            }
            if ((x == cursor_x) ||
                ((cursor_x < (ILI9341_WIDTH - 1U)) &&
                 (x == (uint16_t)(cursor_x + 1U)))) {
                color = RGB565(255, 210, 0);
            }
            if ((y < 2U) || (y >= (ILI9341_HEIGHT - 2U)) ||
                (x < 2U) || (x >= (ILI9341_WIDTH - 2U))) {
                color = RGB565(255, 255, 255);
            }
            g_scanline[x] = color;
        }

        if (!ili9341_draw_rgb565_dma(0, y, ILI9341_WIDTH, 1,
                                      g_scanline)) {
            return false;
        }
    }

    ++test_image_frames_completed;
    return true;
}
