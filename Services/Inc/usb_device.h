#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#define OSC_USB_MAX_FRAME 640U

bool usb_device_init(void);
void usb_device_process(void);
bool usb_device_is_configured(void);
bool usb_device_receive_frame(uint8_t *buffer, uint16_t capacity,
                              uint16_t *length);
bool usb_device_send_frame(const uint8_t *buffer, uint16_t length);
void usb_device_irq_handler(void);

#endif
