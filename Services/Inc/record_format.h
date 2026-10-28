#ifndef RECORD_FORMAT_H
#define RECORD_FORMAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define REC_CHANNELS 3U
#define REC_SETS 512U
#define REC_PAYLOAD (REC_SETS * REC_CHANNELS * 2U)
#define REC_RECORD_SIZE 3584U
#define REC_HEADER_SIZE 512U
#define REC_QUEUE_SLOTS 12U
#define REC_FILE_MAGIC 0x3243534fUL
#define REC_BLOCK_MAGIC 0x324b4c42UL
typedef enum { REC_DATA=1, REC_GAP, REC_ERROR, REC_PAUSE, REC_RESUME,
               REC_SEGMENT_END, REC_SESSION_END } rec_type_t;
typedef struct {
    uint8_t bytes[REC_RECORD_SIZE];
} rec_slot_t;
typedef struct {
    rec_slot_t slots[REC_QUEUE_SLOTS];
    volatile uint32_t producer, consumer, high_water;
    uint32_t producer_slot, consumer_slot;
} rec_queue_t;
typedef struct {
    uint16_t type, flags;
    uint32_t count, epoch, payload_size, session, segment;
    uint64_t sequence, first_index, time_us;
} rec_block_info_t;
void rec_put16(uint8_t *p, uint16_t v);
void rec_put32(uint8_t *p, uint32_t v);
void rec_put64(uint8_t *p, uint64_t v);
uint16_t rec_get16(const uint8_t *p);
uint32_t rec_get32(const uint8_t *p);
uint64_t rec_get64(const uint8_t *p);
uint32_t rec_crc(const void *data, size_t size);
void rec_encode(uint8_t *block, const rec_block_info_t *info);
bool rec_validate(const uint8_t *block, size_t size, rec_block_info_t *info);
bool rec_header_valid(const uint8_t *header);
void rec_queue_reset(rec_queue_t *q);
bool rec_queue_push(rec_queue_t *q, const uint16_t *samples,
                    uint64_t index, uint32_t epoch, uint64_t time_us);
rec_slot_t *rec_queue_peek(rec_queue_t *q);
void rec_queue_pop(rec_queue_t *q);
uint32_t rec_queue_count(const rec_queue_t *q);
#endif
