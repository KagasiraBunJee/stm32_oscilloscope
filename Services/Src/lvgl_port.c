#include "lvgl_port.h"

#include <stdint.h>

#include "board.h"
#include "ili9341.h"
#include "lvgl.h"
#include "stm32f4xx_hal.h"
#include "xpt2046.h"

#define LVGL_DRAW_LINES 20U

static uint16_t g_draw_buffer_a[ILI9341_WIDTH * LVGL_DRAW_LINES]
    __attribute__((aligned(4)));
static uint16_t g_draw_buffer_b[ILI9341_WIDTH * LVGL_DRAW_LINES]
    __attribute__((aligned(4)));
static lv_display_t *g_display;

static void display_flush(lv_display_t *display, const lv_area_t *area,
                          uint8_t *pixels)
{
    uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

    /*
     * Keep the FSMC pixel transfer on DMA, but wait for this small partial
     * buffer before releasing it back to LVGL.  This avoids an initial-frame
     * stall if the board misses the raw DMA2 Stream1 completion interrupt.
     */
    (void)ili9341_draw_rgb565_dma((uint16_t)area->x1,
                                  (uint16_t)area->y1,
                                  width, height,
                                  (const uint16_t *)pixels);
    lv_display_flush_ready(display);
}

static void touch_read(lv_indev_t *input, lv_indev_data_t *data)
{
    xpt2046_point_t point;
    (void)input;
    if (xpt2046_read_pixel(&point)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point.x;
        data->point.y = point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool lvgl_port_init(void)
{
    lv_indev_t *touch;

    ili9341_init();
    xpt2046_init();
    lv_init();
    lv_tick_set_cb(board_millis);

    g_display = lv_display_create(ILI9341_WIDTH, ILI9341_HEIGHT);
    if (g_display == NULL) {
        return false;
    }
    lv_display_set_flush_cb(g_display, display_flush);
    lv_display_set_buffers(g_display, g_draw_buffer_a, g_draw_buffer_b,
                           sizeof(g_draw_buffer_a),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    touch = lv_indev_create();
    if (touch == NULL) {
        return false;
    }
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_read);
    lv_indev_set_display(touch, g_display);
    return true;
}

void lvgl_port_process(void)
{
    (void)lv_timer_handler();
}
