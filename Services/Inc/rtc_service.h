#ifndef RTC_SERVICE_H
#define RTC_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool rtc_service_init(void);
bool rtc_service_is_valid(void);
bool rtc_service_set_utc(uint64_t unix_seconds);
bool rtc_service_get_utc(uint64_t *unix_seconds);
bool rtc_service_format_filename(char *buffer, size_t size,
                                 uint32_t session_id);
uint32_t rtc_service_fattime(void);

#endif
