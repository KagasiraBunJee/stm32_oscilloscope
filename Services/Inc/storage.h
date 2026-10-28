#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Recording service only: fully CRC-validated v1 retention candidate. */
bool storage_legacy_oldest(char *path, size_t capacity, uint32_t *id);

#include "session_format.h"

typedef enum {
    STORAGE_NO_CARD = 0,
    STORAGE_READY,
    STORAGE_SAVING,
    STORAGE_UNSAVED,
    STORAGE_FULL,
    STORAGE_ERROR
} storage_state_t;

typedef enum {
    STORAGE_RESULT_OK = 0,
    STORAGE_RESULT_BUSY,
    STORAGE_RESULT_NO_CARD,
    STORAGE_RESULT_NOT_FOUND,
    STORAGE_RESULT_INVALID,
    STORAGE_RESULT_IO_ERROR,
    STORAGE_RESULT_FULL
} storage_result_t;

void storage_init(void);
bool storage_mount(void);
void storage_process(void);
storage_state_t storage_get_state(void);
storage_state_t storage_status(void);
uint8_t storage_get_progress(void);
bool storage_is_busy(void);
bool storage_has_unsaved_capture(void);
storage_result_t storage_request_save(const osc_capture_t *capture,
                                      osc_stop_reason_t reason);
storage_result_t storage_retry_save(void);
storage_result_t storage_save(const osc_capture_t *capture,
                              osc_stop_reason_t reason);
storage_result_t storage_retry(void);
void storage_discard_unsaved(void);
storage_result_t storage_list(uint32_t cursor, osc_session_info_t *info,
                              uint32_t *next_cursor);
storage_result_t storage_get_info(uint32_t session_id,
                                  osc_session_info_t *info);
storage_result_t storage_read(uint32_t session_id, uint32_t offset,
                              uint8_t *buffer, uint32_t requested,
                              uint32_t *actual);
storage_result_t storage_delete(uint32_t session_id);

#endif
