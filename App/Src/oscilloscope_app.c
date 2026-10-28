#include "oscilloscope_app.h"

#include "acquisition.h"
#include "board.h"
#include "calibration.h"
#include "lvgl_port.h"
#include "oscilloscope_ui.h"
#include "rtc_service.h"
#include "storage.h"
#include "usb_device.h"
#include "usb_protocol.h"
#include "recording.h"

static uint32_t g_heartbeat_at;
static bool g_ui_ready, g_yielding;
/* SWD-readable timing diagnostics; no filesystem work inside UI service. */
volatile uint32_t osc_ui_max_ms, osc_storage_max_ms;

void oscilloscope_app_io_yield(void)
{
    static uint32_t last;
    if (!g_ui_ready || g_yielding || (uint32_t)(board_millis()-last)<5U) return;
    g_yielding=true;
    uint32_t started=board_millis();
    /* Do not decode a new chart frame inside an SD wait: rendering it can
       delay returning a completed write by longer than an ADC block period.
       Touch/status timers still run here; main publishes new display frames. */
    oscilloscope_ui_process();
    lvgl_port_process();
    uint32_t elapsed=board_millis()-started;
    if(elapsed>osc_ui_max_ms)osc_ui_max_ms=elapsed;
    /* Measure the interval from completion, avoiding back-to-back renders. */
    last=board_millis();
    /* USB IRQ reception continues; command dispatch waits until FatFS returns. */
    g_yielding=false;
}

void oscilloscope_app_init(void)
{
    /* D1 (PF9) is active-low and remains lit while firmware is running. */
    board_led_set(0, true);
    board_led_set(1U, false);
    calibration_init();
    recording_init();
    acquisition_init();
    (void)rtc_service_init();
    storage_init();
    usb_protocol_init();
    if (!usb_device_init()) {
        board_led_set(1U, true);
    }
    if (!lvgl_port_init() || !oscilloscope_ui_init()) {
        for (;;) {
            board_led_toggle(1U);
            board_delay_ms(100U);
        }
    }
    g_heartbeat_at = board_millis();
    g_ui_ready=true;
    (void)acquisition_start();
}

void oscilloscope_app_process(void)
{
    uint32_t started=board_millis();
    recording_status_t status;
    /* Drain backlog before display work, bounded so controls keep responding.
       Each recording_process call retains ownership/CRC/gap semantics. */
    do {
        uint32_t before=board_millis();
        recording_process();
        uint32_t elapsed=board_millis()-before;
        if(elapsed>osc_storage_max_ms)osc_storage_max_ms=elapsed;
        recording_status(&status);
    } while(status.queue_depth && status.state!=RECORD_FAILED &&
            (uint32_t)(board_millis()-started)<20U);
    acquisition_process();
    usb_device_process();
    oscilloscope_app_io_yield();
    if ((uint32_t)(board_millis() - g_heartbeat_at) >= 500U) {
        g_heartbeat_at += 500U;
        board_led_toggle(1U);
    }
}
