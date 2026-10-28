#include "oscilloscope_ui.h"

#include <stddef.h>

#include "acquisition.h"
#include "calibration.h"
#include "lvgl.h"
#include "storage.h"
#include "recording.h"
#include "ui_events.h"

#define CHART_POINTS 300U
#define DEFAULT_VIEW_SAMPLES 1024U
#define SCALE_LABEL_COUNT 5U
#define VOLTAGE_HALF_DIVISIONS 2
#define TIME_LABEL_COUNT 3U
#define ROLL_HISTORY_POINTS 512U
#define ROLL_VALUE_NONE INT16_MIN

typedef enum {
    GRAPH_VIEW_ROLL = 0,
    GRAPH_VIEW_WAVE
} graph_view_t;

static lv_obj_t *g_status_label;
static lv_obj_t *g_storage_label;
static lv_obj_t *g_chart;
static lv_chart_series_t *g_series[OSC_MAX_CHANNELS];
static lv_obj_t *g_scale_label[SCALE_LABEL_COUNT];
static lv_obj_t *g_scale_caption;
static lv_obj_t *g_level_label;
static lv_obj_t *g_time_label[TIME_LABEL_COUNT];
static lv_obj_t *g_pause_button;
static lv_obj_t *g_start_button;
static lv_obj_t *g_trigger_button;
static lv_obj_t *g_channel_button[OSC_MAX_CHANNELS];
static lv_obj_t *g_rate_button;
static lv_obj_t *g_view_button;
static lv_obj_t *g_settings_panel;
static lv_obj_t *g_settings_label;
static acquisition_config_t g_ui_config;
static uint32_t g_last_capture_sequence;
static uint32_t g_view_start;
static uint8_t g_rate_index;
static uint8_t g_settings_channel;
static const uint32_t g_channel_colors[] = {0x3DFF60, 0x44B7FF, 0xFF58DB};

static uint8_t g_scale_channel = UINT8_MAX;
static uint32_t g_scale_uv_per_div;
static int32_t g_scale_offset_uv;
static int16_t g_roll_mv[OSC_MAX_CHANNELS][ROLL_HISTORY_POINTS];
static uint32_t g_roll_time_us[ROLL_HISTORY_POINTS];
static uint16_t g_roll_write;
static uint16_t g_roll_count;
static uint16_t g_roll_view_offset;
static graph_view_t g_graph_view = GRAPH_VIEW_ROLL;

static const uint32_t g_sample_rates[] = {
    3300U, 10000U, 100000U, 500000U, 1000000U, 2400000U, 7200000U
};

static lv_obj_t *make_button(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t width, const char *text,
                             lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, 28);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_pad_all(button, 2, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    return button;
}

static void set_button_text(lv_obj_t *button, const char *text)
{
    lv_obj_t *label = lv_obj_get_child(button, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}


static uint8_t primary_logical_channel(void)
{
    uint8_t channel;
    if (g_settings_channel < OSC_MAX_CHANNELS &&
        g_ui_config.channel[g_settings_channel].enabled) return g_settings_channel;
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        if (g_ui_config.channel[channel].enabled) return channel;
    }
    return 0U;
}

static void set_voltage_text(lv_obj_t *label, int32_t microvolts)
{
    uint32_t magnitude = (microvolts < 0)
        ? (uint32_t)(-(int64_t)microvolts) : (uint32_t)microvolts;
    const char *sign = (microvolts < 0) ? "-" : "";

    if (magnitude >= 1000000U) {
        lv_label_set_text_fmt(label, "%s%lu.%02luV", sign,
            (unsigned long)(magnitude / 1000000U),
            (unsigned long)((magnitude / 10000U) % 100U));
    } else {
        lv_label_set_text_fmt(label, "%s%lumV", sign,
            (unsigned long)(magnitude / 1000U));
    }
}

static void update_voltage_scale(uint8_t logical_channel)
{
    const osc_channel_config_t *channel = &g_ui_config.channel[logical_channel];
    uint32_t index;

    if ((g_scale_channel == logical_channel) &&
        (g_scale_uv_per_div == channel->microvolts_per_div) &&
        (g_scale_offset_uv == channel->vertical_offset_uv)) return;

    g_scale_channel = logical_channel;
    g_scale_uv_per_div = channel->microvolts_per_div;
    g_scale_offset_uv = channel->vertical_offset_uv;
    lv_obj_set_style_text_color(g_scale_caption,
        lv_color_hex(g_channel_colors[logical_channel]), 0);
    for (index = 0U; index < SCALE_LABEL_COUNT; ++index) {
        int32_t divisions = VOLTAGE_HALF_DIVISIONS - (int32_t)index;
        int64_t voltage = (int64_t)channel->vertical_offset_uv +
            (int64_t)divisions * channel->microvolts_per_div;
        if (voltage > INT32_MAX) voltage = INT32_MAX;
        if (voltage < INT32_MIN) voltage = INT32_MIN;
        set_voltage_text(g_scale_label[index], (int32_t)voltage);
    }
    if (channel->microvolts_per_div >= 1000000U) {
        lv_label_set_text_fmt(g_scale_caption, "CH%u %luV/div",
            logical_channel + 1U,
            (unsigned long)(channel->microvolts_per_div / 1000000U));
    } else {
        lv_label_set_text_fmt(g_scale_caption, "CH%u %lumV/div",
            logical_channel + 1U,
            (unsigned long)(channel->microvolts_per_div / 1000U));
    }
}

static void update_level_text(const osc_capture_t *capture)
{
    uint8_t logical;
    int32_t average;
    uint32_t magnitude;
    const char *sign;

    if ((capture == NULL) || (capture->channel_count == 0U)) {
        lv_label_set_text(g_level_label, "AVG --");
        return;
    }
    logical = primary_logical_channel();
    uint8_t index;
    for (index = 0U; index < capture->channel_count; ++index)
        if (capture->channel[index].logical_channel == logical) break;
    if (index == capture->channel_count) {
        lv_label_set_text(g_level_label, "AVG --");
        return;
    }
    average = capture->channel[index].measurements.average_uv;
    lv_obj_set_style_text_color(g_level_label,
        lv_color_hex(g_channel_colors[logical]), 0);
    magnitude = (average < 0) ? (uint32_t)(-(int64_t)average)
                              : (uint32_t)average;
    sign = (average < 0) ? "-" : "";
    if (magnitude >= 1000000U) {
        lv_label_set_text_fmt(g_level_label, "CH%u AVG %s%lu.%02luV",
            logical + 1U, sign,
            (unsigned long)(magnitude / 1000000U),
            (unsigned long)((magnitude / 10000U) % 100U));
    } else {
        lv_label_set_text_fmt(g_level_label, "CH%u AVG %s%lumV",
            logical + 1U, sign, (unsigned long)(magnitude / 1000U));
    }
}

static void update_channel_text(uint8_t channel)
{
    static const char *const direct[] = {"CH1 3V3", "CH2 3V3", "CH3 3V3"};
    static const char *const high[] = {"CH1 14V", "CH2 14V", "CH3 14V"};
    static const char *const off[] = {"CH1 OFF", "CH2 OFF", "CH3 OFF"};
    lv_obj_set_style_border_color(g_channel_button[channel],
        lv_color_hex(g_channel_colors[channel]), 0);
    lv_obj_set_style_border_width(g_channel_button[channel],
        channel == g_settings_channel ? 3 : 0, 0);
    if (!g_ui_config.channel[channel].enabled) {
        set_button_text(g_channel_button[channel], off[channel]);
    } else if (g_ui_config.channel[channel].route == OSC_INPUT_HIGH_RANGE) {
        set_button_text(g_channel_button[channel], high[channel]);
    } else {
        set_button_text(g_channel_button[channel], direct[channel]);
    }
}

static void update_trigger_text(void)
{
    const char *text;
    if (g_ui_config.trigger_mode == OSC_TRIGGER_AUTO) {
        text = (g_ui_config.trigger_edge == OSC_TRIGGER_RISING) ? "AUTO +" : "AUTO -";
    } else {
        text = (g_ui_config.trigger_edge == OSC_TRIGGER_RISING) ? "NORM +" : "NORM -";
    }
    set_button_text(g_trigger_button, text);
}

static void update_rate_text(void)
{
    switch (g_sample_rates[g_rate_index]) {
    case 3300U: set_button_text(g_rate_button, "3.3k"); break;
    case 10000U: set_button_text(g_rate_button, "10k"); break;
    case 100000U: set_button_text(g_rate_button, "100k"); break;
    case 500000U: set_button_text(g_rate_button, "500k"); break;
    case 1000000U: set_button_text(g_rate_button, "1M"); break;
    case 2400000U: set_button_text(g_rate_button, "2.4M"); break;
    default: set_button_text(g_rate_button, "7.2M"); break;
    }
}

static void set_time_text(lv_obj_t *label, int64_t nanoseconds)
{
    uint64_t magnitude = (nanoseconds < 0)
        ? (uint64_t)(-nanoseconds) : (uint64_t)nanoseconds;
    const char *sign = (nanoseconds < 0) ? "-" :
                       ((nanoseconds > 0) ? "+" : "");

    if (magnitude >= 1000000000U) {
        lv_label_set_text_fmt(label, "%s%lu.%02lus", sign,
            (unsigned long)(magnitude / 1000000000U),
            (unsigned long)((magnitude / 10000000U) % 100U));
    } else if (magnitude >= 1000000U) {
        lv_label_set_text_fmt(label, "%s%lu.%02lums", sign,
            (unsigned long)(magnitude / 1000000U),
            (unsigned long)((magnitude / 10000U) % 100U));
    } else if (magnitude >= 1000U) {
        lv_label_set_text_fmt(label, "%s%lu.%02luus", sign,
            (unsigned long)(magnitude / 1000U),
            (unsigned long)((magnitude / 10U) % 100U));
    } else {
        lv_label_set_text_fmt(label, "%s%luns", sign,
            (unsigned long)magnitude);
    }
}

static void roll_history_reset(void)
{
    g_roll_write = 0U;
    g_roll_count = 0U;
    g_roll_view_offset = 0U;
}

static void roll_history_append(const osc_capture_t *capture)
{
    uint32_t channel;
    uint64_t elapsed_us;

    if ((capture == NULL) || (capture->actual_sample_rate_hz == 0U)) return;
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        g_roll_mv[channel][g_roll_write] = ROLL_VALUE_NONE;
    }
    for (channel = 0U; channel < capture->channel_count; ++channel) {
        uint8_t logical = capture->channel[channel].logical_channel;
        int32_t millivolts = capture->channel[channel].measurements.average_uv /
            1000;
        if (millivolts > INT16_MAX) millivolts = INT16_MAX;
        if (millivolts <= INT16_MIN) millivolts = INT16_MIN + 1;
        g_roll_mv[logical][g_roll_write] = (int16_t)millivolts;
    }
    elapsed_us = (uint64_t)capture->sequence * capture->sample_count *
        1000000ULL / capture->actual_sample_rate_hz;
    g_roll_time_us[g_roll_write] = (elapsed_us > UINT32_MAX)
        ? UINT32_MAX : (uint32_t)elapsed_us;
    g_roll_write = (uint16_t)((g_roll_write + 1U) % ROLL_HISTORY_POINTS);
    if (g_roll_count < ROLL_HISTORY_POINTS) ++g_roll_count;
    g_roll_view_offset = 0U;
}

static uint16_t roll_oldest_index(void)
{
    return (uint16_t)((g_roll_write + ROLL_HISTORY_POINTS - g_roll_count) %
                      ROLL_HISTORY_POINTS);
}

static void update_roll_time_axis(uint16_t window_start,
                                  uint16_t window_count)
{
    uint16_t oldest;
    uint16_t latest;
    uint32_t label;

    if (window_count == 0U) {
        for (label = 0U; label < TIME_LABEL_COUNT; ++label) {
            lv_label_set_text(g_time_label[label], "--");
        }
        return;
    }
    oldest = roll_oldest_index();
    latest = (uint16_t)((g_roll_write + ROLL_HISTORY_POINTS - 1U) %
                        ROLL_HISTORY_POINTS);
    for (label = 0U; label < TIME_LABEL_COUNT; ++label) {
        uint16_t offset = (uint16_t)(window_start +
            (label * (window_count - 1U)) / (TIME_LABEL_COUNT - 1U));
        uint16_t index = (uint16_t)((oldest + offset) % ROLL_HISTORY_POINTS);
        int64_t relative_ns = ((int64_t)g_roll_time_us[index] -
                               g_roll_time_us[latest]) * 1000LL;
        set_time_text(g_time_label[label], relative_ns);
    }
}

static void update_roll_chart(const osc_capture_t *capture)
{
    uint16_t oldest;
    uint16_t window_count;
    uint16_t newest_window_start;
    uint16_t window_start;
    uint32_t channel;
    uint32_t point;

    if (capture != NULL) {
        update_voltage_scale(primary_logical_channel());
        update_level_text(capture);
    }
    oldest = roll_oldest_index();
    window_count = (g_roll_count < CHART_POINTS)
        ? g_roll_count : CHART_POINTS;
    if (lv_chart_get_point_count(g_chart) != CHART_POINTS) {
        lv_chart_set_point_count(g_chart, CHART_POINTS);
    }
    newest_window_start = g_roll_count - window_count;
    if (g_roll_view_offset > newest_window_start) {
        g_roll_view_offset = newest_window_start;
    }
    window_start = newest_window_start - g_roll_view_offset;

    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        int32_t *values = lv_chart_get_y_array(g_chart, g_series[channel]);
        lv_chart_hide_series(g_chart, g_series[channel],
                             !g_ui_config.channel[channel].enabled);
        for (point = 0U; point < CHART_POINTS; ++point) {
            if ((point < window_count) ||
                ((window_count == 1U) && (point == 1U))) {
                uint16_t history_point = (window_count == 1U) ? 0U :
                    (uint16_t)point;
                uint16_t index = (uint16_t)((oldest + window_start +
                                             history_point) %
                                            ROLL_HISTORY_POINTS);
                int16_t millivolts = g_roll_mv[channel][index];
                if (millivolts != ROLL_VALUE_NONE) {
                    int64_t scaled = ((int64_t)millivolts * 1000LL -
                        g_ui_config.channel[channel].vertical_offset_uv) * 1024LL /
                        (int64_t)g_ui_config.channel[channel].microvolts_per_div;
                    if (scaled > 2047) scaled = 2047;
                    if (scaled < -2048) scaled = -2048;
                    values[point] = (int32_t)scaled;
                } else {
                    values[point] = LV_CHART_POINT_NONE;
                }
            } else {
                values[point] = LV_CHART_POINT_NONE;
            }
        }
    }
    update_roll_time_axis(window_start, window_count);
    lv_chart_refresh(g_chart);
}

static uint32_t view_sample_count(const osc_capture_t *capture)
{
    uint32_t count;

    if (capture == NULL) return DEFAULT_VIEW_SAMPLES;
    count = (capture->actual_sample_rate_hz <= 10000U)
        ? capture->sample_count : DEFAULT_VIEW_SAMPLES;
    if (count > capture->sample_count) count = capture->sample_count;
    return (count != 0U) ? count : 1U;
}

static void update_time_axis(const osc_capture_t *capture,
                             uint32_t view_samples)
{
    uint32_t index;
    uint32_t reference_sample;

    if ((capture == NULL) || (capture->actual_sample_rate_hz == 0U)) return;
    reference_sample = capture->trigger_valid ? capture->trigger_index
                                               : g_view_start;
    for (index = 0U; index < TIME_LABEL_COUNT; ++index) {
        uint32_t sample = g_view_start +
            (index * (view_samples - 1U)) / (TIME_LABEL_COUNT - 1U);
        int32_t delta = (int32_t)sample - (int32_t)reference_sample;
        int64_t nanoseconds = (int64_t)delta * 1000000000LL /
            capture->actual_sample_rate_hz;
        set_time_text(g_time_label[index], nanoseconds);
    }
}

static void update_chart(const osc_capture_t *capture)
{
    uint32_t point;
    uint32_t channel;
    uint32_t view_samples;

    if (g_graph_view == GRAPH_VIEW_ROLL) {
        update_roll_chart(capture);
        return;
    }
    if (capture == NULL) return;
    if (lv_chart_get_point_count(g_chart) != CHART_POINTS) {
        lv_chart_set_point_count(g_chart, CHART_POINTS);
    }
    view_samples = view_sample_count(capture);
    update_voltage_scale(primary_logical_channel());
    update_level_text(capture);
    if (g_view_start > capture->sample_count - view_samples) {
        g_view_start = capture->sample_count - view_samples;
    }
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        int32_t *values = lv_chart_get_y_array(g_chart, g_series[channel]);
        lv_chart_hide_series(g_chart, g_series[channel],
                             channel >= capture->channel_count);
        for (point = 0U; point < CHART_POINTS; ++point) {
            if (channel < capture->channel_count) {
                uint32_t sample = g_view_start +
                    (point * (view_samples - 1U)) / (CHART_POINTS - 1U);
                uint8_t logical = capture->channel[channel].logical_channel;
                const calibration_path_t path = {
                    capture->channel[channel].gain_uv_per_lsb_q16,
                    capture->channel[channel].offset_uv
                };
                int32_t uv = calibration_apply(&path,
                    capture->samples[channel][sample]);
                int64_t scaled = ((int64_t)uv -
                    g_ui_config.channel[logical].vertical_offset_uv) * 1024LL /
                    (int64_t)g_ui_config.channel[logical].microvolts_per_div;
                if (scaled > 2047) scaled = 2047;
                if (scaled < -2048) scaled = -2048;
                values[point] = (int32_t)scaled;
            } else {
                values[point] = LV_CHART_POINT_NONE;
            }
        }
    }
    update_time_axis(capture, view_samples);
    lv_chart_refresh(g_chart);
}

static void update_settings_text(void)
{
    const osc_capture_t *capture = acquisition_get_frozen_capture();
    const osc_measurements_t *measurement = NULL;
    uint32_t capture_channel;
    for (capture_channel = 0U; capture != NULL &&
         capture_channel < capture->channel_count; ++capture_channel) {
        if (capture->channel[capture_channel].logical_channel == g_settings_channel) {
            measurement = &capture->channel[capture_channel].measurements;
            break;
        }
    }
    if (measurement != NULL) {
        lv_label_set_text_fmt(g_settings_label,
            "%s  CH%u  %lumV/div  offset %ldmV\nTrig %u  position %u%%\nmin %ldmV max %ldmV avg %ldmV\nRMS %lumV  Vpp %lumV  %luHz / %luns",
            (g_graph_view == GRAPH_VIEW_ROLL) ? "ROLL" : "WAVE",
            g_settings_channel + 1U,
            (unsigned long)(g_ui_config.channel[g_settings_channel].microvolts_per_div / 1000U),
            (long)(g_ui_config.channel[g_settings_channel].vertical_offset_uv / 1000),
            g_ui_config.trigger_level_raw,
            (unsigned)((uint32_t)g_ui_config.trigger_position * 100U / REC_SETS),
            (long)(measurement->minimum_uv / 1000),
            (long)(measurement->maximum_uv / 1000),
            (long)(measurement->average_uv / 1000),
            (unsigned long)(measurement->rms_uv / 1000U),
            (unsigned long)(measurement->peak_to_peak_uv / 1000U),
            (unsigned long)(measurement->frequency_millihz / 1000U),
            (unsigned long)measurement->period_ns);
    } else {
        lv_label_set_text_fmt(g_settings_label,
            "%s  CH%u  %lumV/div  offset %ldmV\nTrig %u  position %u%%\nNo capture for this channel",
            (g_graph_view == GRAPH_VIEW_ROLL) ? "ROLL" : "WAVE",
            g_settings_channel + 1U,
            (unsigned long)(g_ui_config.channel[g_settings_channel].microvolts_per_div / 1000U),
            (long)(g_ui_config.channel[g_settings_channel].vertical_offset_uv / 1000),
            g_ui_config.trigger_level_raw,
            (unsigned)((uint32_t)g_ui_config.trigger_position * 100U / REC_SETS));
    }
}

enum {
    SET_TRIG_DOWN, SET_TRIG_UP, SET_POS_DOWN, SET_POS_UP, SET_CHANNEL,
    SET_SCALE_DOWN, SET_SCALE_UP, SET_OFFSET_DOWN, SET_OFFSET_UP,
    SET_VIEW, SET_CLOSE
};

static void setting_adjusted(lv_event_t *event)
{
    static const uint32_t scales[] = {100000U, 200000U, 500000U, 825000U,
                                      1000000U, 2000000U, 4000000U, 5000000U};
    uint32_t action = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    uint32_t scale_index = 0U;
    if (action == SET_CLOSE) {
        lv_obj_add_flag(g_settings_panel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (action == SET_CHANNEL) {
        g_settings_channel = (uint8_t)((g_settings_channel + 1U) % OSC_MAX_CHANNELS);
        for (uint8_t i = 0U; i < OSC_MAX_CHANNELS; ++i) update_channel_text(i);
        update_chart(acquisition_get_frozen_capture());
        update_settings_text();
        return;
    }
    if (action == SET_VIEW) {
        g_graph_view = (g_graph_view == GRAPH_VIEW_ROLL)
            ? GRAPH_VIEW_WAVE : GRAPH_VIEW_ROLL;
        g_roll_view_offset = 0U;
        set_button_text(g_view_button,
            (g_graph_view == GRAPH_VIEW_ROLL) ? "ROLL" : "WAVE");
        update_settings_text();
        update_chart(acquisition_get_frozen_capture());
        return;
    }
    while (scale_index + 1U < sizeof(scales) / sizeof(scales[0]) &&
           scales[scale_index] < g_ui_config.channel[g_settings_channel].microvolts_per_div) {
        ++scale_index;
    }
    switch (action) {
    case SET_TRIG_DOWN:
        g_ui_config.trigger_level_raw = (g_ui_config.trigger_level_raw > 128U)
            ? g_ui_config.trigger_level_raw - 128U : 0U; break;
    case SET_TRIG_UP:
        g_ui_config.trigger_level_raw = (g_ui_config.trigger_level_raw < 3967U)
            ? g_ui_config.trigger_level_raw + 128U : 4095U; break;
    case SET_POS_DOWN:
        g_ui_config.trigger_position = (g_ui_config.trigger_position > 32U)
            ? g_ui_config.trigger_position - 32U : 0U; break;
    case SET_POS_UP:
        g_ui_config.trigger_position = (g_ui_config.trigger_position < REC_SETS - 33U)
            ? g_ui_config.trigger_position + 32U : REC_SETS - 1U; break;
    case SET_SCALE_DOWN:
        if (scale_index != 0U) --scale_index;
        g_ui_config.channel[g_settings_channel].microvolts_per_div = scales[scale_index]; break;
    case SET_SCALE_UP:
        if (scale_index + 1U < sizeof(scales) / sizeof(scales[0])) ++scale_index;
        g_ui_config.channel[g_settings_channel].microvolts_per_div = scales[scale_index]; break;
    case SET_OFFSET_DOWN:
        g_ui_config.channel[g_settings_channel].vertical_offset_uv -=
            (int32_t)(g_ui_config.channel[g_settings_channel].microvolts_per_div / 2U); break;
    case SET_OFFSET_UP:
        g_ui_config.channel[g_settings_channel].vertical_offset_uv +=
            (int32_t)(g_ui_config.channel[g_settings_channel].microvolts_per_div / 2U); break;
    default: break;
    }
    acquisition_set_display(&g_ui_config);
    update_settings_text();
    update_chart(acquisition_get_frozen_capture());
}

static void settings_clicked(lv_event_t *event)
{
    (void)event;
    update_settings_text();
    lv_obj_remove_flag(g_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_settings_panel);
}

static void start_clicked(lv_event_t *event)
{
    (void)event;
    (void)recording_start();
}
static void pause_clicked(lv_event_t *event)
{
    (void)event;
    recording_status_t status;recording_status(&status);
    if(status.state==RECORD_PAUSED)(void)recording_resume();
    else (void)recording_pause();
}
static void stop_clicked(lv_event_t *event)
{
    (void)event;
    (void)recording_stop();
}
static void trigger_clicked(lv_event_t *event)
{
    (void)event;
    g_ui_config.trigger_edge = g_ui_config.trigger_edge==OSC_TRIGGER_RISING?
        OSC_TRIGGER_FALLING:OSC_TRIGGER_RISING;
    acquisition_set_display(&g_ui_config);
    update_trigger_text();
}
static void channel_clicked(lv_event_t *event)
{
    /* All three physical divider inputs stay enabled. Select the view channel. */
    g_settings_channel=(uint8_t)(uintptr_t)lv_event_get_user_data(event);
    for (uint8_t i = 0U; i < OSC_MAX_CHANNELS; ++i) update_channel_text(i);
    update_voltage_scale(g_settings_channel);
    update_level_text(acquisition_get_frozen_capture());
}
static void rate_clicked(lv_event_t *event)
{
    (void)event; /* Fixed 10 kS/s simultaneous recording profile. */
}

static void chart_dragged(lv_event_t *event)
{
    const osc_capture_t *capture = acquisition_get_frozen_capture();
    uint32_t view_samples;
    lv_point_t vector;
    int32_t start;
    int32_t roll_offset;
    (void)event;
    if (acquisition_get_state() != ACQUISITION_PAUSED &&
        acquisition_get_state() != ACQUISITION_STOPPED) return;
    if (capture == NULL) return;
    lv_indev_get_vect(lv_indev_active(), &vector);
    if (g_graph_view == GRAPH_VIEW_ROLL) {
        int32_t maximum = (g_roll_count > CHART_POINTS)
            ? (int32_t)(g_roll_count - CHART_POINTS) : 0;
        roll_offset = (int32_t)g_roll_view_offset + vector.x;
        if (roll_offset < 0) roll_offset = 0;
        if (roll_offset > maximum) roll_offset = maximum;
        g_roll_view_offset = (uint16_t)roll_offset;
        update_roll_chart(capture);
        return;
    }
    view_samples = view_sample_count(capture);
    if (view_samples >= capture->sample_count) return;
    start = (int32_t)g_view_start - vector.x * 4;
    if (start < 0) start = 0;
    if (start > (int32_t)(capture->sample_count - view_samples)) {
        start = (int32_t)(capture->sample_count - view_samples);
    }
    g_view_start = (uint32_t)start;
    update_chart(acquisition_get_frozen_capture());
}

bool oscilloscope_ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *setting_button;
    uint32_t channel;

    g_ui_config = *acquisition_get_config();
    roll_history_reset();
    g_rate_index = 1U; /* 10 kS/s for the installed unbuffered dividers. */
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x071018), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xD8EAF0), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    g_status_label = lv_label_create(screen);
    lv_obj_set_pos(g_status_label, 4, 2);
    lv_obj_set_width(g_status_label, 230);
    lv_label_set_text(g_status_label, "LIVE PC0-PC2 10 kS/s");
    g_storage_label = lv_label_create(screen);
    lv_obj_set_pos(g_storage_label, 246, 2);
    lv_obj_set_width(g_storage_label, 70);
    lv_obj_set_style_text_align(g_storage_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(g_storage_label, "SD --");

    g_chart = lv_chart_create(screen);
    lv_obj_set_pos(g_chart, 0, 20);
    lv_obj_set_size(g_chart, 320, 134);
    lv_obj_set_style_bg_color(g_chart, lv_color_hex(0x020608), 0);
    lv_obj_set_style_border_width(g_chart, 0, 0);
    lv_obj_set_style_pad_all(g_chart, 0, 0);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, CHART_POINTS);
    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, -2048, 2048);
    lv_chart_set_div_line_count(g_chart, 5, 7);
    lv_obj_set_style_line_color(g_chart, lv_color_hex(0x29414C),
                                LV_PART_MAIN);
    lv_obj_set_style_line_opa(g_chart, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_line_dash_width(g_chart, 4, LV_PART_MAIN);
    lv_obj_set_style_line_dash_gap(g_chart, 4, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(g_chart, true, LV_PART_ITEMS);
    lv_obj_set_style_width(g_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(g_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g_chart, 1, LV_PART_MAIN);
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        g_series[channel] = lv_chart_add_series(g_chart, lv_color_hex(g_channel_colors[channel]),
                                                LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_hide_series(g_chart, g_series[channel],
                             !g_ui_config.channel[channel].enabled);
    }
    for (channel = 0U; channel < SCALE_LABEL_COUNT; ++channel) {
        int32_t line_y = (133 * (int32_t)channel) /
                         (SCALE_LABEL_COUNT - 1U);
        int32_t label_y = line_y - 6;
        if (label_y < 0) label_y = 0;
        if (label_y > 120) label_y = 120;
        g_scale_label[channel] = lv_label_create(g_chart);
        lv_obj_set_pos(g_scale_label[channel], 2, label_y);
        lv_obj_set_height(g_scale_label[channel], 14);
        lv_obj_set_style_text_font(g_scale_label[channel],
                                   &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(g_scale_label[channel],
                                    lv_color_hex(0xA9C1C9), 0);
        lv_obj_set_style_bg_color(g_scale_label[channel],
                                  lv_color_hex(0x020608), 0);
        lv_obj_set_style_bg_opa(g_scale_label[channel], LV_OPA_70, 0);
        lv_obj_set_style_pad_hor(g_scale_label[channel], 1, 0);
        lv_obj_remove_flag(g_scale_label[channel], LV_OBJ_FLAG_CLICKABLE);
    }
    g_scale_caption = lv_label_create(g_chart);
    lv_obj_set_pos(g_scale_caption, 216, 2);
    lv_obj_set_width(g_scale_caption, 100);
    lv_obj_set_style_text_align(g_scale_caption, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(g_scale_caption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_scale_caption, lv_color_hex(0x3DFF60), 0);
    lv_obj_set_style_bg_color(g_scale_caption, lv_color_hex(0x020608), 0);
    lv_obj_set_style_bg_opa(g_scale_caption, LV_OPA_70, 0);
    lv_obj_remove_flag(g_scale_caption, LV_OBJ_FLAG_CLICKABLE);
    g_level_label = lv_label_create(g_chart);
    lv_obj_set_pos(g_level_label, 176, 102);
    lv_obj_set_width(g_level_label, 140);
    lv_obj_set_style_text_align(g_level_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(g_level_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_level_label, lv_color_hex(0xD8EAF0), 0);
    lv_obj_set_style_bg_color(g_level_label, lv_color_hex(0x020608), 0);
    lv_obj_set_style_bg_opa(g_level_label, LV_OPA_70, 0);
    lv_obj_remove_flag(g_level_label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(g_level_label, "AVG --");
    for (channel = 0U; channel < TIME_LABEL_COUNT; ++channel) {
        static const int32_t positions[] = {2, 128, 254};
        g_time_label[channel] = lv_label_create(screen);
        lv_obj_set_pos(g_time_label[channel], positions[channel], 156);
        lv_obj_set_width(g_time_label[channel], 64);
        lv_obj_set_style_text_align(g_time_label[channel],
            (channel == 0U) ? LV_TEXT_ALIGN_LEFT :
            ((channel == TIME_LABEL_COUNT - 1U) ? LV_TEXT_ALIGN_RIGHT
                                                : LV_TEXT_ALIGN_CENTER), 0);
        lv_obj_set_style_text_font(g_time_label[channel],
                                   &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(g_time_label[channel],
                                    lv_color_hex(0x88A6AF), 0);
        lv_obj_set_style_bg_color(g_time_label[channel],
                                  lv_color_hex(0x020608), 0);
        lv_obj_set_style_bg_opa(g_time_label[channel], LV_OPA_70, 0);
        lv_obj_remove_flag(g_time_label[channel], LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(g_time_label[channel], "--");
    }
    update_voltage_scale(primary_logical_channel());
    lv_obj_add_event_cb(g_chart, chart_dragged, LV_EVENT_PRESSING, NULL);

    g_start_button = make_button(screen, 4, 174, 62, "START", start_clicked);
    g_pause_button = make_button(screen, 70, 174, 62, "PAUSE", pause_clicked);
    (void)make_button(screen, 136, 174, 62, "STOP", stop_clicked);
    g_trigger_button = make_button(screen, 202, 174, 114, "AUTO +", trigger_clicked);
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        g_channel_button[channel] = make_button(screen,
            4 + (int32_t)channel * 66, 206, 62, "", channel_clicked);
        lv_obj_remove_event_cb(g_channel_button[channel], channel_clicked);
        lv_obj_add_event_cb(g_channel_button[channel], channel_clicked,
                            LV_EVENT_CLICKED, (void *)(uintptr_t)channel);
        update_channel_text((uint8_t)channel);
    }
    g_rate_button = make_button(screen, 202, 206, 54, "", rate_clicked);
    update_rate_text();
    (void)make_button(screen, 260, 206, 56, "SET", settings_clicked);

    g_settings_panel = lv_obj_create(screen);
    lv_obj_set_pos(g_settings_panel, 4, 20);
    lv_obj_set_size(g_settings_panel, 312, 216);
    lv_obj_set_style_bg_color(g_settings_panel, lv_color_hex(0x101D25), 0);
    lv_obj_set_style_border_color(g_settings_panel, lv_color_hex(0x44B7FF), 0);
    lv_obj_set_style_pad_all(g_settings_panel, 4, 0);
    g_settings_label = lv_label_create(g_settings_panel);
    lv_obj_set_pos(g_settings_label, 4, 2);
    lv_obj_set_width(g_settings_label, 298);
    setting_button = make_button(g_settings_panel, 4, 108, 70, "TRIG -", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_TRIG_DOWN);
    setting_button = make_button(g_settings_panel, 78, 108, 70, "TRIG +", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_TRIG_UP);
    setting_button = make_button(g_settings_panel, 152, 108, 70, "POS -", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_POS_DOWN);
    setting_button = make_button(g_settings_panel, 226, 108, 70, "POS +", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_POS_UP);
    setting_button = make_button(g_settings_panel, 4, 142, 54, "CH", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_CHANNEL);
    setting_button = make_button(g_settings_panel, 62, 142, 54, "V/D-", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_SCALE_DOWN);
    setting_button = make_button(g_settings_panel, 120, 142, 54, "V/D+", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_SCALE_UP);
    setting_button = make_button(g_settings_panel, 178, 142, 54, "OFF-", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_OFFSET_DOWN);
    setting_button = make_button(g_settings_panel, 236, 142, 54, "OFF+", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_OFFSET_UP);
    g_view_button = make_button(g_settings_panel, 4, 176, 90, "ROLL",
                                setting_adjusted);
    lv_obj_remove_event_cb(g_view_button, setting_adjusted);
    lv_obj_add_event_cb(g_view_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_VIEW);
    setting_button = make_button(g_settings_panel, 110, 176, 90, "CLOSE", setting_adjusted);
    lv_obj_remove_event_cb(setting_button, setting_adjusted);
    lv_obj_add_event_cb(setting_button, setting_adjusted,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)SET_CLOSE);
    lv_obj_add_flag(g_settings_panel, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void oscilloscope_ui_process(void)
{
    const osc_capture_t *capture=acquisition_get_frozen_capture();
    recording_status_t status;recording_status(&status);
    ui_event_t event;
    while(ui_get_event(&event)) { /* Status is sampled below, not inferred from events. */ }
    static uint32_t last_status;
    if((uint32_t)(HAL_GetTick()-last_status)>=100U) {
        last_status=HAL_GetTick();
        static const char *names[]={"LIVE","PREPARING","PREPARING","REC","PAUSING","PAUSED","FINALIZING","REC ERROR"};
        const char *name=status.state<=RECORD_FAILED?names[status.state]:"ERROR";
        if(status.state==RECORD_FAILED)
            lv_label_set_text_fmt(g_status_label,"REC ERROR %lu - %s",(unsigned long)status.error,
                acquisition_get_state()==ACQUISITION_RUNNING?"LIVE":"ADC ERR");
        else if(status.state==RECORD_ACTIVE)
            lv_label_set_text_fmt(g_status_label,"REC %lus lost %lu",(unsigned long)(status.elapsed_us/1000000ULL),(unsigned long)status.dropped_sets);
        else lv_label_set_text_fmt(g_status_label,"%s 3CH 10kS/s",name);
        if(recording_busy())lv_obj_add_state(g_start_button,LV_STATE_DISABLED);
        else lv_obj_remove_state(g_start_button,LV_STATE_DISABLED);
        set_button_text(g_pause_button,status.state==RECORD_PAUSED?"RESUME":"PAUSE");
        if(status.state==RECORD_FAILED)oscilloscope_ui_set_storage_status("SD ERR");
        else if(recording_busy())oscilloscope_ui_set_storage_status("SD REC");
        else oscilloscope_ui_set_storage_status(storage_get_state()==STORAGE_READY?"SD OK":"SD --");
    }
    if(capture && capture->sequence!=g_last_capture_sequence) {
        g_last_capture_sequence=capture->sequence;
        roll_history_append(capture);
        update_chart(capture);
        if(!lv_obj_has_flag(g_settings_panel,LV_OBJ_FLAG_HIDDEN))update_settings_text();
    }
}

void oscilloscope_ui_set_storage_status(const char *text)
{
    lv_label_set_text(g_storage_label, (text != NULL) ? text : "SD --");
}

void oscilloscope_ui_set_save_progress(uint8_t percent)
{
    lv_label_set_text_fmt(g_storage_label, "SD %u%%", percent);
}
