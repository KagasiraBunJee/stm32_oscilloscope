#ifndef RECORDING_H
#define RECORDING_H
#include "record_format.h"
typedef enum { RECORD_IDLE, RECORD_PREPARING, RECORD_ARMED, RECORD_ACTIVE,
    RECORD_PAUSING, RECORD_PAUSED, RECORD_FINALIZING, RECORD_FAILED } recording_state_t;
typedef enum { RECORD_OK, RECORD_NO_CARD, RECORD_IO_ERROR, RECORD_FULL, RECORD_ADC_ERROR } recording_error_t;
typedef struct {
    uint32_t state, error, session, segment, queue_high_water, queue_depth;
    uint64_t recorded_sets, dropped_sets, unknown_gaps, records, durable_bytes, elapsed_us;
} recording_status_t;
void recording_init(void);
bool recording_start(void);
bool recording_pause(void);
bool recording_resume(void);
bool recording_stop(void);
void recording_process(void);
bool recording_busy(void);
void recording_status(recording_status_t *status);
/* ADC ISR only; returns true when a requested pause reached a block boundary. */
bool recording_ingest(const uint16_t *samples, uint64_t index, uint32_t epoch, uint64_t time_us);
void recording_adc_error(void);
uint64_t recording_clock_us(void);
void recording_io_yield(void);
/* V2 segment browsing: fixed numeric names, no caller-controlled filesystem paths. */
int recording_list(uint32_t cursor,uint8_t *info,uint32_t *next);
int recording_read(uint32_t session,uint32_t segment,uint32_t offset,uint8_t *data,uint32_t length,uint32_t *actual);
int recording_delete(uint32_t session);
#endif
