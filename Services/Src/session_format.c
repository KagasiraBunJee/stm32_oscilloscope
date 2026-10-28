#include "session_format.h"

#include <string.h>

_Static_assert(sizeof(osc_session_header_v1_t) == OSC_SESSION_HEADER_SIZE,
               "OSC v1 header must be exactly 256 bytes");

uint32_t session_crc32(const void *data, size_t length, uint32_t previous)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = ~previous;
    while (length-- != 0U) {
        uint32_t bit;
        crc ^= *bytes++;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320UL &
                  (uint32_t)-(int32_t)(crc & 1UL));
        }
    }
    return ~crc;
}

size_t session_encode_payload_chunk(const osc_capture_t *capture,
                                    uint32_t byte_offset, uint8_t *output,
                                    size_t capacity)
{
    uint32_t item;
    uint32_t total_items;
    size_t written = 0U;
    if ((capture == NULL) || (output == NULL) ||
        ((byte_offset & 1U) != 0U) || (capture->channel_count == 0U)) {
        return 0U;
    }
    total_items = (uint32_t)capture->sample_count * capture->channel_count;
    item = byte_offset / 2U;
    while ((item < total_items) && (written + 2U <= capacity)) {
        uint32_t sample = item / capture->channel_count;
        uint32_t channel = item % capture->channel_count;
        uint16_t value = capture->samples[channel][sample];
        output[written++] = (uint8_t)value;
        output[written++] = (uint8_t)(value >> 8);
        ++item;
    }
    return written;
}

bool session_build_header(osc_session_header_v1_t *header,
                          const osc_capture_t *capture, uint32_t session_id,
                          uint64_t timestamp_utc, bool rtc_valid,
                          osc_stop_reason_t reason)
{
    uint8_t payload_buffer[256];
    uint32_t offset = 0U;
    uint32_t channel;
    uint32_t payload_crc = 0U;

    if ((header == NULL) || (capture == NULL) ||
        (capture->channel_count == 0U) ||
        (capture->channel_count > OSC_MAX_CHANNELS)) return false;

    memset(header, 0, sizeof(*header));
    header->magic = OSC_SESSION_MAGIC;
    header->version = OSC_SESSION_VERSION;
    header->header_size = OSC_SESSION_HEADER_SIZE;
    header->session_id = session_id;
    header->timestamp_utc = timestamp_utc;
    header->flags = (rtc_valid ? OSC_SESSION_FLAG_RTC_VALID : 0U) |
                    (capture->trigger_valid ? OSC_SESSION_FLAG_TRIGGER_VALID : 0U);
    header->firmware_version = OSC_FIRMWARE_VERSION;
    header->sample_rate_hz = capture->actual_sample_rate_hz;
    header->sample_count = capture->sample_count;
    header->trigger_index = capture->trigger_index;
    header->trigger_level_raw = capture->trigger_level_raw;
    header->channel_count = capture->channel_count;
    header->data_layout = 1U; /* Interleaved little-endian uint16. */
    header->trigger_mode = (uint8_t)capture->trigger_mode;
    header->trigger_edge = (uint8_t)capture->trigger_edge;
    header->stop_reason = (uint8_t)reason;

    for (channel = 0U; channel < capture->channel_count; ++channel) {
        const osc_capture_channel_t *source = &capture->channel[channel];
        osc_session_channel_v1_t *destination = &header->channel[channel];
        destination->logical_channel = source->logical_channel;
        destination->route = (uint8_t)source->route;
        destination->adc_channel = source->adc_channel;
        destination->gain_uv_per_lsb_q16 = source->gain_uv_per_lsb_q16;
        destination->offset_uv = source->offset_uv;
        destination->minimum_uv = source->measurements.minimum_uv;
        destination->maximum_uv = source->measurements.maximum_uv;
        destination->average_uv = source->measurements.average_uv;
        destination->rms_uv = source->measurements.rms_uv;
        destination->peak_to_peak_uv = source->measurements.peak_to_peak_uv;
        destination->frequency_millihz = source->measurements.frequency_millihz;
        destination->period_ns = source->measurements.period_ns;
    }

    header->payload_size = (uint32_t)capture->sample_count *
                           capture->channel_count * sizeof(uint16_t);
    header->total_length = header->header_size + header->payload_size;
    while (offset < header->payload_size) {
        size_t length = session_encode_payload_chunk(capture, offset,
                                                     payload_buffer,
                                                     sizeof(payload_buffer));
        if (length == 0U) return false;
        payload_crc = session_crc32(payload_buffer, length, payload_crc);
        offset += (uint32_t)length;
    }
    header->payload_crc32 = payload_crc;
    header->header_crc32 = 0U;
    header->header_crc32 = session_crc32(header, sizeof(*header), 0U);
    return true;
}

bool session_validate_header(const osc_session_header_v1_t *header,
                             uint32_t file_size)
{
    osc_session_header_v1_t copy;
    uint32_t stored_crc;
    if ((header == NULL) || (header->magic != OSC_SESSION_MAGIC) ||
        (header->version != OSC_SESSION_VERSION) ||
        (header->header_size != OSC_SESSION_HEADER_SIZE) ||
        (header->channel_count == 0U) ||
        (header->channel_count > OSC_MAX_CHANNELS) ||
        (header->sample_count != OSC_SAMPLES_PER_CHANNEL) ||
        (header->data_layout != 1U) ||
        (header->payload_size != (uint32_t)header->sample_count *
         header->channel_count * sizeof(uint16_t)) ||
        (header->total_length != header->header_size + header->payload_size) ||
        (file_size != header->total_length)) return false;
    copy = *header;
    stored_crc = copy.header_crc32;
    copy.header_crc32 = 0U;
    return stored_crc == session_crc32(&copy, sizeof(copy), 0U);
}
