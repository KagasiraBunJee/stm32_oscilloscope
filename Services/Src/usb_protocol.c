#include "usb_protocol.h"

#include <stdbool.h>
#include <string.h>

#include "acquisition.h"
#include "rtc_service.h"
#include "session_format.h"
#include "storage.h"
#include "usb_device.h"
#include "recording.h"

#define USB_CAP_INTERLEAVED_1CH (1UL << 0)
#define USB_CAP_SIMULTANEOUS_2CH (1UL << 1)
#define USB_CAP_SIMULTANEOUS_3CH (1UL << 2)
#define USB_CAP_SD_SESSIONS (1UL << 3)
#define USB_CAP_RTC_SYNC (1UL << 4)
#define USB_CAP_TOUCH_UI (1UL << 5)

typedef struct __attribute__((packed)) {
    uint32_t firmware_version;
    uint32_t capabilities;
    uint32_t max_one_channel_rate_hz;
    uint32_t max_multi_channel_rate_hz;
    uint16_t samples_per_channel;
    uint8_t max_channels;
    uint8_t acquisition_state;
    uint8_t storage_state;
    uint8_t rtc_valid;
    uint8_t usb_packet_size;
    uint8_t reserved;
} device_info_response_t;

typedef struct __attribute__((packed)) {
    uint32_t sample_rate_hz;
    uint16_t trigger_level_raw;
    uint16_t trigger_position;
    uint8_t trigger_mode;
    uint8_t trigger_edge;
    uint8_t enabled_mask;
    uint8_t high_range_mask;
    int32_t vertical_offset_uv[OSC_MAX_CHANNELS];
    uint32_t microvolts_per_div[OSC_MAX_CHANNELS];
} configure_request_t;

typedef struct __attribute__((packed)) {
    uint32_t sequence;
    uint32_t sample_rate_hz;
    uint16_t sample_count;
    uint16_t trigger_index;
    uint16_t trigger_level_raw;
    uint8_t channel_count;
    uint8_t trigger_valid;
    uint8_t trigger_mode;
    uint8_t trigger_edge;
    uint8_t state;
    uint8_t reserved[5];
} capture_info_response_t;

typedef struct __attribute__((packed)) {
    uint32_t expected_sequence;
    uint16_t sample_offset;
    uint16_t sample_count;
    uint8_t capture_channel;
    uint8_t reserved[3];
} capture_read_request_t;

typedef struct __attribute__((packed)) {
    uint32_t sequence;
    uint16_t sample_offset;
    uint16_t sample_count;
    uint8_t capture_channel;
    uint8_t reserved[3];
} capture_read_response_t;

typedef struct __attribute__((packed)) {
    uint8_t state;
    uint8_t progress;
    uint8_t has_unsaved_capture;
    uint8_t rtc_valid;
} storage_status_response_t;

typedef struct __attribute__((packed)) {
    uint32_t session_id;
    uint32_t offset;
    uint16_t length;
    uint16_t reserved;
} session_read_request_t;

typedef struct __attribute__((packed)) {
    uint32_t session_id;
    uint32_t offset;
    uint16_t length;
    uint16_t reserved;
} session_read_response_t;

typedef struct __attribute__((packed)) {
    uint32_t session_id;
    uint64_t timestamp_utc;
    uint32_t total_length;
    uint32_t sample_rate_hz;
    uint16_t sample_count;
    uint8_t channel_count;
    uint8_t flags;
    char filename[64];
} session_info_response_t;

typedef struct __attribute__((packed)) {
    uint32_t sequence;
    uint8_t channel_count;
    uint8_t reserved[3];
    osc_session_channel_v1_t channel[OSC_MAX_CHANNELS];
} measurements_response_t;

static uint8_t g_rx[OSC_USB_MAX_FRAME] __attribute__((aligned(4)));
static uint8_t g_tx[OSC_USB_MAX_FRAME] __attribute__((aligned(4)));

_Static_assert(sizeof(osc_usb_frame_header_t) == OSC_USB_PROTOCOL_HEADER_SIZE,
               "USB protocol header size changed");
_Static_assert(sizeof(capture_info_response_t) == 24U,
               "Capture info wire size changed");
_Static_assert(sizeof(session_info_response_t) == 88U,
               "Session info wire size changed");
_Static_assert(sizeof(osc_session_channel_v1_t) == 40U,
               "Measurement record wire size changed");

static uint32_t frame_crc(const osc_usb_frame_header_t *header,
                          const uint8_t *payload)
{
    osc_usb_frame_header_t copy = *header;
    uint32_t crc;
    copy.crc32 = 0U;
    crc = session_crc32(&copy, sizeof(copy), 0U);
    return session_crc32(payload, copy.payload_length, crc);
}

static osc_usb_status_t map_storage_result(storage_result_t result)
{
    switch (result) {
    case STORAGE_RESULT_OK: return OSC_USB_STATUS_OK;
    case STORAGE_RESULT_BUSY: return OSC_USB_STATUS_BUSY;
    case STORAGE_RESULT_NO_CARD: return OSC_USB_STATUS_NO_CARD;
    case STORAGE_RESULT_NOT_FOUND: return OSC_USB_STATUS_NOT_FOUND;
    case STORAGE_RESULT_FULL: return OSC_USB_STATUS_STORAGE_FULL;
    case STORAGE_RESULT_INVALID: return OSC_USB_STATUS_INVALID_ARGUMENT;
    default: return OSC_USB_STATUS_IO_ERROR;
    }
}

static void session_info_to_wire(session_info_response_t *wire,
                                 const osc_session_info_t *info)
{
    memset(wire, 0, sizeof(*wire));
    wire->session_id = info->session_id;
    wire->timestamp_utc = info->timestamp_utc;
    wire->total_length = info->total_length;
    wire->sample_rate_hz = info->sample_rate_hz;
    wire->sample_count = info->sample_count;
    wire->channel_count = info->channel_count;
    wire->flags = info->flags;
    memcpy(wire->filename, info->filename, sizeof(wire->filename));
}

static void send_response(const osc_usb_frame_header_t *request,
                          osc_usb_status_t status, const void *payload,
                          uint32_t payload_length)
{
    osc_usb_frame_header_t *response = (osc_usb_frame_header_t *)g_tx;
    if (payload_length > OSC_USB_MAX_PAYLOAD) {
        status = OSC_USB_STATUS_BAD_LENGTH;
        payload = NULL;
        payload_length = 0U;
    }
    memset(response, 0, sizeof(*response));
    response->magic = OSC_USB_PROTOCOL_MAGIC;
    response->version = request->version == 1U ? 1U : OSC_USB_PROTOCOL_VERSION;
    response->header_size = OSC_USB_PROTOCOL_HEADER_SIZE;
    response->command = request->command;
    response->status = (uint16_t)status;
    response->flags = 1U; /* Response frame. */
    response->sequence = request->sequence;
    response->payload_length = payload_length;
    if (payload_length != 0U && payload != NULL) {
        if (payload != &g_tx[sizeof(*response)]) {
            memcpy(&g_tx[sizeof(*response)], payload, payload_length);
        }
    }
    response->crc32 = frame_crc(response, &g_tx[sizeof(*response)]);
    (void)usb_device_send_frame(g_tx,
        (uint16_t)(sizeof(*response) + payload_length));
}

static bool request_size(const osc_usb_frame_header_t *request,
                         uint32_t expected)
{
    return request->payload_length == expected;
}

static osc_usb_status_t handle_configure(const uint8_t *payload,
                                         uint32_t length)
{
    configure_request_t wire;
    acquisition_config_t config;
    uint32_t channel;
    if (length != sizeof(wire)) return OSC_USB_STATUS_BAD_LENGTH;
    if (recording_busy()) return OSC_USB_STATUS_BUSY;
    memcpy(&wire, payload, sizeof(wire));
    memset(&config, 0, sizeof(config));
    if (wire.enabled_mask != 7U || wire.high_range_mask != 7U || wire.sample_rate_hz != 10000U ||
        (wire.trigger_mode > OSC_TRIGGER_NORMAL) ||
        (wire.trigger_edge > OSC_TRIGGER_FALLING)) {
        return OSC_USB_STATUS_INVALID_ARGUMENT;
    }
    config.requested_sample_rate_hz = wire.sample_rate_hz;
    config.trigger_level_raw = wire.trigger_level_raw;
    config.trigger_position = wire.trigger_position;
    config.trigger_mode = (osc_trigger_mode_t)wire.trigger_mode;
    config.trigger_edge = (osc_trigger_edge_t)wire.trigger_edge;
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        config.channel[channel].enabled = (wire.enabled_mask & (1U << channel)) != 0U;
        config.channel[channel].route = (wire.high_range_mask & (1U << channel))
            ? OSC_INPUT_HIGH_RANGE : OSC_INPUT_DIRECT;
        config.channel[channel].vertical_offset_uv = wire.vertical_offset_uv[channel];
        config.channel[channel].microvolts_per_div = wire.microvolts_per_div[channel];
    }
    if(config.trigger_level_raw>4095U || config.trigger_position>=REC_SETS)
        return OSC_USB_STATUS_INVALID_ARGUMENT;
    for(unsigned i=0;i<3;i++)if(!config.channel[i].microvolts_per_div)return OSC_USB_STATUS_INVALID_ARGUMENT;
    acquisition_set_display(&config);
    return OSC_USB_STATUS_OK;
}

static osc_usb_status_t control_start(void)
{
    return recording_start()?OSC_USB_STATUS_OK:OSC_USB_STATUS_BUSY;
}
static osc_usb_status_t control_stop(void)
{
    return recording_stop()?OSC_USB_STATUS_OK:OSC_USB_STATUS_FAILED;
}

static void handle_request(const osc_usb_frame_header_t *request,
                           const uint8_t *payload)
{
    if(request->version==1U && ((request->command>=0x10U && request->command<=0x14U) || request->command>=0x40U)) {
        send_response(request,OSC_USB_STATUS_BAD_VERSION,NULL,0);return;
    }
    switch ((osc_usb_command_t)request->command) {
    case OSC_USB_CMD_DEVICE_INFO: {
        device_info_response_t info = {
            OSC_FIRMWARE_VERSION,
            (1UL<<6) | USB_CAP_SIMULTANEOUS_3CH | USB_CAP_SD_SESSIONS |
            USB_CAP_RTC_SYNC | USB_CAP_TOUCH_UI,
            10000U, 10000U, REC_SETS,
            OSC_MAX_CHANNELS, (uint8_t)acquisition_get_state(),
            (uint8_t)storage_get_state(), rtc_service_is_valid() ? 1U : 0U,
            64U, 0U
        };
        if (!request_size(request, 0U)) send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U);
        else send_response(request, OSC_USB_STATUS_OK, &info, sizeof(info));
        break;
    }
    case OSC_USB_CMD_CONFIGURE:
        send_response(request, handle_configure(payload, request->payload_length), NULL, 0U);
        break;
    case OSC_USB_CMD_START:
        send_response(request, request_size(request, 0U) ? control_start() : OSC_USB_STATUS_BAD_LENGTH, NULL, 0U);
        break;
    case OSC_USB_CMD_PAUSE:
        send_response(request, !request_size(request, 0U) ? OSC_USB_STATUS_BAD_LENGTH :
            (recording_pause() ? OSC_USB_STATUS_OK : OSC_USB_STATUS_FAILED), NULL, 0U);
        break;
    case OSC_USB_CMD_RESUME:
        send_response(request, !request_size(request, 0U) ? OSC_USB_STATUS_BAD_LENGTH :
            (recording_resume() ? OSC_USB_STATUS_OK : OSC_USB_STATUS_FAILED), NULL, 0U);
        break;
    case OSC_USB_CMD_STOP:
        send_response(request, request_size(request, 0U) ? control_stop() : OSC_USB_STATUS_BAD_LENGTH, NULL, 0U);
        break;
    case OSC_USB_CMD_CAPTURE_INFO: {
        const osc_capture_t *capture = acquisition_get_frozen_capture();
        capture_info_response_t info;
        if (!request_size(request, 0U)) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        if (capture == NULL) { send_response(request, OSC_USB_STATUS_NO_CAPTURE, NULL, 0U); break; }
        memset(&info, 0, sizeof(info));
        info.sequence = capture->sequence;
        info.sample_rate_hz = capture->actual_sample_rate_hz;
        info.sample_count = capture->sample_count;
        info.trigger_index = capture->trigger_index;
        info.trigger_level_raw = capture->trigger_level_raw;
        info.channel_count = capture->channel_count;
        info.trigger_valid = capture->trigger_valid;
        info.trigger_mode = (uint8_t)capture->trigger_mode;
        info.trigger_edge = (uint8_t)capture->trigger_edge;
        info.state = (uint8_t)acquisition_get_state();
        send_response(request, OSC_USB_STATUS_OK, &info, sizeof(info));
        break;
    }
    case OSC_USB_CMD_CAPTURE_READ: {
        capture_read_request_t read_request;
        capture_read_response_t read_response;
        const osc_capture_t *capture = acquisition_get_frozen_capture();
        uint32_t byte_count;
        if (!request_size(request, sizeof(read_request))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&read_request, payload, sizeof(read_request));
        if (capture == NULL) { send_response(request, OSC_USB_STATUS_NO_CAPTURE, NULL, 0U); break; }
        if (read_request.capture_channel >= capture->channel_count ||
            read_request.sample_offset >= capture->sample_count ||
            read_request.expected_sequence != capture->sequence) {
            send_response(request, OSC_USB_STATUS_INVALID_ARGUMENT, NULL, 0U); break;
        }
        if (read_request.sample_count > (OSC_USB_MAX_PAYLOAD - sizeof(read_response)) / 2U) {
            read_request.sample_count = (OSC_USB_MAX_PAYLOAD - sizeof(read_response)) / 2U;
        }
        if (read_request.sample_count > capture->sample_count - read_request.sample_offset) {
            read_request.sample_count = capture->sample_count - read_request.sample_offset;
        }
        read_response.sequence = capture->sequence;
        read_response.sample_offset = read_request.sample_offset;
        read_response.sample_count = read_request.sample_count;
        read_response.capture_channel = read_request.capture_channel;
        memset(read_response.reserved, 0, sizeof(read_response.reserved));
        memcpy(&g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], &read_response, sizeof(read_response));
        byte_count = (uint32_t)read_request.sample_count * 2U;
        memcpy(&g_tx[OSC_USB_PROTOCOL_HEADER_SIZE + sizeof(read_response)],
               &capture->samples[read_request.capture_channel][read_request.sample_offset], byte_count);
        send_response(request, OSC_USB_STATUS_OK,
                      &g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], sizeof(read_response) + byte_count);
        break;
    }
    case OSC_USB_CMD_MEASUREMENTS: {
        const osc_capture_t *capture = acquisition_get_frozen_capture();
        measurements_response_t response;
        uint32_t channel;
        uint32_t length;
        if (!request_size(request, 0U)) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        if (capture == NULL) { send_response(request, OSC_USB_STATUS_NO_CAPTURE, NULL, 0U); break; }
        memset(&response, 0, sizeof(response));
        response.sequence = capture->sequence;
        response.channel_count = capture->channel_count;
        for (channel = 0U; channel < capture->channel_count; ++channel) {
            const osc_capture_channel_t *source = &capture->channel[channel];
            osc_session_channel_v1_t *destination = &response.channel[channel];
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
        length = 8U + capture->channel_count * sizeof(response.channel[0]);
        send_response(request, OSC_USB_STATUS_OK, &response, length);
        break;
    }
    case OSC_USB_CMD_STORAGE_STATUS: {
        storage_status_response_t status = {
            (uint8_t)storage_get_state(), storage_get_progress(),
            storage_has_unsaved_capture() ? 1U : 0U,
            rtc_service_is_valid() ? 1U : 0U
        };
        send_response(request, request_size(request, 0U) ? OSC_USB_STATUS_OK : OSC_USB_STATUS_BAD_LENGTH,
                      request_size(request, 0U) ? &status : NULL,
                      request_size(request, 0U) ? sizeof(status) : 0U);
        break;
    }
    case OSC_USB_CMD_RTC_SET: {
        uint64_t timestamp;
        if (!request_size(request, sizeof(timestamp))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&timestamp, payload, sizeof(timestamp));
        send_response(request, rtc_service_set_utc(timestamp) ? OSC_USB_STATUS_OK : OSC_USB_STATUS_INVALID_ARGUMENT, NULL, 0U);
        break;
    }
    case OSC_USB_CMD_SESSION_LIST: {
        uint32_t cursor, next;
        osc_session_info_t info;
        session_info_response_t wire;
        storage_result_t result;
        if (!request_size(request, sizeof(cursor))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&cursor, payload, sizeof(cursor));
        result = storage_list(cursor, &info, &next);
        if (result == STORAGE_RESULT_OK) {
            session_info_to_wire(&wire, &info);
            memcpy(&g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], &next, sizeof(next));
            memcpy(&g_tx[OSC_USB_PROTOCOL_HEADER_SIZE + sizeof(next)], &wire, sizeof(wire));
            send_response(request, OSC_USB_STATUS_OK, &g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], sizeof(next) + sizeof(wire));
        } else send_response(request, map_storage_result(result), NULL, 0U);
        break;
    }
    case OSC_USB_CMD_SESSION_INFO: {
        uint32_t id;
        osc_session_info_t info;
        session_info_response_t wire;
        storage_result_t result;
        if (!request_size(request, sizeof(id))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&id, payload, sizeof(id));
        result = storage_get_info(id, &info);
        if (result == STORAGE_RESULT_OK) session_info_to_wire(&wire, &info);
        send_response(request, map_storage_result(result), result == STORAGE_RESULT_OK ? &wire : NULL,
                      result == STORAGE_RESULT_OK ? sizeof(wire) : 0U);
        break;
    }
    case OSC_USB_CMD_SESSION_READ: {
        session_read_request_t read_request;
        session_read_response_t response;
        uint32_t actual = 0U;
        storage_result_t result;
        if (!request_size(request, sizeof(read_request))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&read_request, payload, sizeof(read_request));
        if (read_request.length > OSC_USB_MAX_PAYLOAD - sizeof(response)) {
            read_request.length = OSC_USB_MAX_PAYLOAD - sizeof(response);
        }
        result = storage_read(read_request.session_id, read_request.offset,
            &g_tx[OSC_USB_PROTOCOL_HEADER_SIZE + sizeof(response)], read_request.length, &actual);
        if (result != STORAGE_RESULT_OK) { send_response(request, map_storage_result(result), NULL, 0U); break; }
        response.session_id = read_request.session_id;
        response.offset = read_request.offset;
        response.length = (uint16_t)actual;
        response.reserved = 0U;
        memcpy(&g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], &response, sizeof(response));
        send_response(request, OSC_USB_STATUS_OK, &g_tx[OSC_USB_PROTOCOL_HEADER_SIZE], sizeof(response) + actual);
        break;
    }
    case OSC_USB_CMD_SESSION_DELETE: {
        uint32_t id;
        if (!request_size(request, sizeof(id))) { send_response(request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); break; }
        memcpy(&id, payload, sizeof(id));
        send_response(request, map_storage_result(storage_delete(id)), NULL, 0U);
        break;
    }
    case OSC_USB_CMD_RECORDING_STATUS: {
        if(!request_size(request,0)){send_response(request,OSC_USB_STATUS_BAD_LENGTH,NULL,0);break;}
        recording_status_t s;recording_status(&s);uint8_t wire[72];
        rec_put32(wire,s.state);rec_put32(wire+4,s.error);rec_put32(wire+8,s.session);
        rec_put32(wire+12,s.segment);rec_put32(wire+16,s.queue_high_water);rec_put32(wire+20,s.queue_depth);
        rec_put64(wire+24,s.recorded_sets);rec_put64(wire+32,s.dropped_sets);rec_put64(wire+40,s.unknown_gaps);
        rec_put64(wire+48,s.records);rec_put64(wire+56,s.durable_bytes);rec_put64(wire+64,s.elapsed_us);
        send_response(request,OSC_USB_STATUS_OK,wire,sizeof(wire));break;
    }
    case OSC_USB_CMD_SEGMENT_LIST: {
        if(!request_size(request,4)){send_response(request,OSC_USB_STATUS_BAD_LENGTH,NULL,0);break;}
        uint8_t wire[68];uint32_t next=0;
        storage_result_t result=(storage_result_t)recording_list(rec_get32(payload),wire+4,&next);
        rec_put32(wire,next);
        send_response(request,map_storage_result(result),result==STORAGE_RESULT_OK?wire:NULL,result==STORAGE_RESULT_OK?68:0);break;
    }
    case OSC_USB_CMD_SEGMENT_READ: {
        if(!request_size(request,16)){send_response(request,OSC_USB_STATUS_BAD_LENGTH,NULL,0);break;}
        uint32_t length=rec_get32(payload+12),actual=0;
        if(length>OSC_USB_MAX_PAYLOAD-16)length=OSC_USB_MAX_PAYLOAD-16;
        uint8_t *wire=g_tx+OSC_USB_PROTOCOL_HEADER_SIZE;
        storage_result_t result=(storage_result_t)recording_read(rec_get32(payload),rec_get32(payload+4),
            rec_get32(payload+8),wire+16,length,&actual);
        memcpy(wire,payload,12);rec_put32(wire+12,actual);
        send_response(request,map_storage_result(result),result==STORAGE_RESULT_OK?wire:NULL,result==STORAGE_RESULT_OK?16+actual:0);break;
    }
    case OSC_USB_CMD_RECORDING_DELETE: {
        if(!request_size(request,4)){send_response(request,OSC_USB_STATUS_BAD_LENGTH,NULL,0);break;}
        send_response(request,map_storage_result((storage_result_t)recording_delete(rec_get32(payload))),NULL,0);break;
    }
    default:
        send_response(request, OSC_USB_STATUS_UNKNOWN_COMMAND, NULL, 0U);
        break;
    }
}

void usb_protocol_init(void)
{
    memset(g_rx, 0, sizeof(g_rx));
    memset(g_tx, 0, sizeof(g_tx));
}

void usb_protocol_process(void)
{
    osc_usb_frame_header_t request;
    uint16_t received;
    if (!usb_device_receive_frame(g_rx, sizeof(g_rx), &received)) return;
    if (received < sizeof(request)) return;
    memcpy(&request, g_rx, sizeof(request));
    if (request.magic != OSC_USB_PROTOCOL_MAGIC) {
        send_response(&request, OSC_USB_STATUS_BAD_MAGIC, NULL, 0U); return;
    }
    if ((request.version != OSC_USB_PROTOCOL_VERSION && request.version != 1U) ||
        request.header_size != sizeof(request)) {
        send_response(&request, OSC_USB_STATUS_BAD_VERSION, NULL, 0U); return;
    }
    if (request.payload_length > OSC_USB_MAX_PAYLOAD ||
        received != sizeof(request) + request.payload_length) {
        send_response(&request, OSC_USB_STATUS_BAD_LENGTH, NULL, 0U); return;
    }
    if (request.crc32 != frame_crc(&request, &g_rx[sizeof(request)])) {
        send_response(&request, OSC_USB_STATUS_BAD_CRC, NULL, 0U); return;
    }
    handle_request(&request, &g_rx[sizeof(request)]);
}
