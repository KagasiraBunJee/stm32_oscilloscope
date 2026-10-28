#include "xpt2046.h"

#include "ili9341.h"
#include "touch_bus.h"

#define TOUCH_RAW_MIN 200U
#define TOUCH_RAW_MAX 3900U
#define TOUCH_Z1_MIN  100U
#define TOUCH_Z2_MAX  4000U
#define TOUCH_PRESSURE_MIN 200U
#define TOUCH_PROBE_MIN 50U
#define TOUCH_PROBE_MAX 4045U

volatile uint16_t xpt2046_last_z1;
volatile uint16_t xpt2046_last_z2;
volatile uint16_t xpt2046_probe_x;
volatile uint16_t xpt2046_probe_y;
volatile uint32_t xpt2046_press_count;

static uint16_t read_channel(uint8_t command)
{
    uint16_t value;

    touch_bus_select(true);
    (void)touch_bus_transfer(command);
    value = (uint16_t)touch_bus_transfer(0x00) << 8;
    value |= touch_bus_transfer(0x00);
    touch_bus_select(false);
    return (uint16_t)((value >> 3) & 0x0FFFU);
}

static uint16_t clamp_raw(uint16_t value)
{
    if (value < TOUCH_RAW_MIN) {
        return TOUCH_RAW_MIN;
    }
    if (value > TOUCH_RAW_MAX) {
        return TOUCH_RAW_MAX;
    }
    return value;
}

void xpt2046_init(void)
{
    touch_bus_init();

    /*
     * Finish one conversion with PD1:PD0 = 00.  This powers down the ADC and
     * reference and explicitly enables PENIRQ instead of assuming its reset
     * state.
     */
    (void)read_channel(0x90U);
    xpt2046_last_z1 = 0U;
    xpt2046_last_z2 = 4095U;
    xpt2046_probe_x = 0U;
    xpt2046_probe_y = 0U;
    xpt2046_press_count = 0U;
}

bool xpt2046_is_pressed(void)
{
    uint16_t z1;
    uint16_t z2;
    uint16_t x;
    uint16_t y;
    uint32_t pressure;

    if (touch_bus_pen_is_down()) {
        ++xpt2046_press_count;
        return true;
    }

    /* Permit operation if PENIRQ is unavailable but the SPI touch data works. */
    z1 = read_channel(0xB0U);
    z2 = read_channel(0xC0U);
    xpt2046_last_z1 = z1;
    xpt2046_last_z2 = z2;
    pressure = (uint32_t)z1 + (4095U - z2);
    if ((z1 > TOUCH_Z1_MIN) && (z2 < TOUCH_Z2_MAX) &&
        (pressure > TOUCH_PRESSURE_MIN)) {
        ++xpt2046_press_count;
        return true;
    }

    /*
     * Some compatible modules do not expose a working PENIRQ/Z pair.  An
     * untouched four-wire panel drives the coordinate readings to a rail;
     * two non-rail conversions are therefore a useful final fallback.
     */
    x = read_channel(0x90U);
    y = read_channel(0xD0U);
    if ((x > TOUCH_PROBE_MIN) && (x < TOUCH_PROBE_MAX) &&
        (y > TOUCH_PROBE_MIN) && (y < TOUCH_PROBE_MAX)) {
        xpt2046_probe_x = x;
        xpt2046_probe_y = y;
        ++xpt2046_press_count;
        return true;
    }
    return false;
}

bool xpt2046_read_raw(xpt2046_point_t *point)
{
    uint32_t x = 0UL;
    uint32_t y = 0UL;
    uint32_t sample;

    if ((point == 0) || !xpt2046_is_pressed()) {
        return false;
    }

    (void)read_channel(0x90U);
    for (sample = 0; sample < 5UL; ++sample) {
        x += read_channel(0x90U); /* X position */
        y += read_channel(0xD0U); /* Y position */
    }
    point->x = (uint16_t)(x / 5UL);
    point->y = (uint16_t)(y / 5UL);
    /* Retain the last real touch sample for bench calibration via SWD. */
    xpt2046_probe_x = point->x;
    xpt2046_probe_y = point->y;
    return xpt2046_is_pressed();
}

bool xpt2046_read_pixel(xpt2046_point_t *point)
{
    xpt2046_point_t raw;
    uint32_t x;
    uint32_t y;

    if ((point == 0) || !xpt2046_read_raw(&raw)) {
        return false;
    }

    /*
     * Approximate landscape mapping matching ILI9341 MADCTL 0x68.
     * Replace the raw limits with per-panel calibration values after the
     * wiring and orientation have been verified.
     */
    raw.x = clamp_raw(raw.x);
    raw.y = clamp_raw(raw.y);
    x = ((uint32_t)(TOUCH_RAW_MAX - raw.x) * (ILI9341_WIDTH - 1U)) /
        (TOUCH_RAW_MAX - TOUCH_RAW_MIN);
    y = ((uint32_t)(raw.y - TOUCH_RAW_MIN) * (ILI9341_HEIGHT - 1U)) /
        (TOUCH_RAW_MAX - TOUCH_RAW_MIN);
    point->x = (uint16_t)x;
    point->y = (uint16_t)y;
    return true;
}
