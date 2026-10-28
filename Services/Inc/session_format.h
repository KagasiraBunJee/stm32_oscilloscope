#ifndef SESSION_FORMAT_H
#define SESSION_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "acquisition.h"

#define OSC_SESSION_MAGIC 0x3143534FUL
#define OSC_SESSION_VERSION 1U
#define OSC_SESSION_HEADER_SIZE 256U
#define OSC_FIRMWARE_VERSION 0x00020000UL /* 2.0: continuous three-channel recording */

#define OSC_SESSION_FLAG_RTC_VALID (1UL << 0)
#define OSC_SESSION_FLAG_TRIGGER_VALID (1UL << 1)

typedef enum {
    OSC_STOP_REASON_UI = 0,
    OSC_STOP_REASON_USB = 1
} osc_stop_reason_t;

typedef struct __attribute__((packed)) {
    uint8_t logical_channel;
    uint8_t route;
    uint8_t adc_channel;
    uint8_t reserved0;
    int32_t gain_uv_per_lsb_q16;
    int32_t offset_uv;
    int32_t minimum_uv;
    int32_t maximum_uv;
    int32_t average_uv;
    uint32_t rms_uv;
    uint32_t peak_to_peak_uv;
    uint32_t frequency_millihz;
    uint32_t period_ns;
} osc_session_channel_v1_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_length;
    uint32_t session_id;
    uint64_t timestamp_utc;
    uint32_t flags;
    uint32_t firmware_version;
    uint32_t sample_rate_hz;
    uint16_t sample_count;
    uint16_t trigger_index;
    uint16_t trigger_level_raw;
    uint8_t channel_count;
    uint8_t data_layout;
    uint8_t trigger_mode;
    uint8_t trigger_edge;
    uint8_t stop_reason;
    uint8_t reserved0;
    osc_session_channel_v1_t channel[OSC_MAX_CHANNELS];
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t header_crc32;
    uint8_t reserved[76];
} osc_session_header_v1_t;

typedef struct {
    uint32_t session_id;
    uint64_t timestamp_utc;
    uint32_t total_length;
    uint32_t sample_rate_hz;
    uint16_t sample_count;
    uint8_t channel_count;
    uint8_t flags;
    char filename[64];
} osc_session_info_t;

uint32_t session_crc32(const void *data, size_t length, uint32_t previous);
bool session_build_header(osc_session_header_v1_t *header,
                          const osc_capture_t *capture, uint32_t session_id,
                          uint64_t timestamp_utc, bool rtc_valid,
                          osc_stop_reason_t reason);
bool session_validate_header(const osc_session_header_v1_t *header,
                             uint32_t file_size);
size_t session_encode_payload_chunk(const osc_capture_t *capture,
                                    uint32_t byte_offset, uint8_t *output,
                                    size_t capacity);

#endif
