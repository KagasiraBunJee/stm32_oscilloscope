#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include <stdint.h>

#define OSC_USB_PROTOCOL_MAGIC 0x3150534FUL /* "OSP1" */
#define OSC_USB_PROTOCOL_VERSION 2U
#define OSC_USB_PROTOCOL_HEADER_SIZE 24U
#define OSC_USB_MAX_PAYLOAD 600U

typedef enum {
    OSC_USB_CMD_DEVICE_INFO = 0x0001,
    OSC_USB_CMD_CONFIGURE = 0x0010,
    OSC_USB_CMD_START = 0x0011,
    OSC_USB_CMD_PAUSE = 0x0012,
    OSC_USB_CMD_RESUME = 0x0013,
    OSC_USB_CMD_STOP = 0x0014,
    OSC_USB_CMD_CAPTURE_INFO = 0x0020,
    OSC_USB_CMD_CAPTURE_READ = 0x0021,
    OSC_USB_CMD_MEASUREMENTS = 0x0022,
    OSC_USB_CMD_STORAGE_STATUS = 0x0030,
    OSC_USB_CMD_RTC_SET = 0x0031,
    OSC_USB_CMD_SESSION_LIST = 0x0032,
    OSC_USB_CMD_SESSION_INFO = 0x0033,
    OSC_USB_CMD_SESSION_READ = 0x0034,
    OSC_USB_CMD_SESSION_DELETE = 0x0035,
    OSC_USB_CMD_RECORDING_STATUS = 0x0040,
    OSC_USB_CMD_SEGMENT_LIST = 0x0041,
    OSC_USB_CMD_SEGMENT_READ = 0x0042,
    OSC_USB_CMD_RECORDING_DELETE = 0x0043
} osc_usb_command_t;

typedef enum {
    OSC_USB_STATUS_OK = 0,
    OSC_USB_STATUS_BAD_MAGIC = 1,
    OSC_USB_STATUS_BAD_VERSION = 2,
    OSC_USB_STATUS_BAD_LENGTH = 3,
    OSC_USB_STATUS_BAD_CRC = 4,
    OSC_USB_STATUS_UNKNOWN_COMMAND = 5,
    OSC_USB_STATUS_INVALID_ARGUMENT = 6,
    OSC_USB_STATUS_BUSY = 7,
    OSC_USB_STATUS_NO_CAPTURE = 8,
    OSC_USB_STATUS_NO_CARD = 9,
    OSC_USB_STATUS_NOT_FOUND = 10,
    OSC_USB_STATUS_IO_ERROR = 11,
    OSC_USB_STATUS_STORAGE_FULL = 12,
    OSC_USB_STATUS_FAILED = 13
} osc_usb_status_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t header_size;
    uint16_t command;
    uint16_t status;
    uint16_t flags;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t crc32;
} osc_usb_frame_header_t;

void usb_protocol_init(void);
void usb_protocol_process(void);

#endif
