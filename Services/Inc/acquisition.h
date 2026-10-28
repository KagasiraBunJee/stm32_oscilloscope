#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define OSC_MAX_CHANNELS 3U
#define OSC_SAMPLES_PER_CHANNEL 4096U
#define OSC_DIVIDER_MAX_RATE_HZ 10000U

typedef enum {
    OSC_INPUT_DIRECT = 0,
    OSC_INPUT_HIGH_RANGE = 1 /* PC0-PC2: positive 0-14 V via 47k/10k. */
} osc_input_route_t;

typedef enum {
    OSC_TRIGGER_AUTO = 0,
    OSC_TRIGGER_NORMAL = 1
} osc_trigger_mode_t;

typedef enum {
    OSC_TRIGGER_RISING = 0,
    OSC_TRIGGER_FALLING = 1
} osc_trigger_edge_t;

typedef enum {
    ACQUISITION_IDLE = 0,
    ACQUISITION_RUNNING,
    ACQUISITION_PAUSED,
    ACQUISITION_STOPPED,
    ACQUISITION_ERROR
} acquisition_state_t;

typedef struct {
    bool enabled;
    osc_input_route_t route;
    int32_t vertical_offset_uv;
    uint32_t microvolts_per_div;
} osc_channel_config_t;

typedef struct {
    osc_channel_config_t channel[OSC_MAX_CHANNELS];
    uint32_t requested_sample_rate_hz;
    osc_trigger_mode_t trigger_mode;
    osc_trigger_edge_t trigger_edge;
    uint16_t trigger_level_raw;
    uint16_t trigger_position;
} acquisition_config_t;

typedef struct {
    int32_t minimum_uv;
    int32_t maximum_uv;
    int32_t average_uv;
    uint32_t rms_uv;
    uint32_t peak_to_peak_uv;
    uint32_t frequency_millihz;
    uint32_t period_ns;
} osc_measurements_t;

typedef struct {
    uint8_t logical_channel;
    osc_input_route_t route;
    uint8_t adc_channel;
    int32_t gain_uv_per_lsb_q16;
    int32_t offset_uv;
    osc_measurements_t measurements;
} osc_capture_channel_t;

typedef struct {
    uint32_t sequence;
    uint32_t actual_sample_rate_hz;
    uint16_t sample_count;
    uint16_t trigger_index;
    uint16_t trigger_level_raw;
    uint8_t channel_count;
    uint8_t trigger_valid;
    osc_trigger_mode_t trigger_mode;
    osc_trigger_edge_t trigger_edge;
    osc_capture_channel_t channel[OSC_MAX_CHANNELS];
    uint16_t samples[OSC_MAX_CHANNELS][OSC_SAMPLES_PER_CHANNEL];
} osc_capture_t;

void acquisition_init(void);
uint32_t acquisition_max_sample_rate(const acquisition_config_t *config);
bool acquisition_configure(const acquisition_config_t *config);
void acquisition_set_display(const acquisition_config_t *config);
bool acquisition_start(void);
bool acquisition_pause(void);
bool acquisition_resume(void);
bool acquisition_stop(void);
void acquisition_process(void);
acquisition_state_t acquisition_get_state(void);
const acquisition_config_t *acquisition_get_config(void);
const osc_capture_t *acquisition_get_frozen_capture(void);
bool acquisition_has_capture(void);
DMA_HandleTypeDef *acquisition_dma_handle(void);
void acquisition_adc_irq(void);

#endif
