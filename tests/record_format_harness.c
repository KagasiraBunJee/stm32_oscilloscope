#include "record_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static rec_queue_t queue;
static uint16_t input[REC_SETS*3];
static uint8_t encoded[REC_RECORD_SIZE];
int main(void)
{
    rec_queue_reset(&queue);
    /* Wrap the monotonic counters at a non-multiple of the twelve-slot ring. */
    queue.producer=UINT32_MAX-5;queue.consumer=UINT32_MAX-5;
    for(unsigned cycle=0;cycle<20;cycle++) {
        for(unsigned block=0;block<12;block++) {
            for(unsigned i=0;i<REC_SETS;i++)for(unsigned c=0;c<3;c++)input[i*3+c]=(uint16_t)((i+c*1000+block)&4095);
            assert(rec_queue_push(&queue,input,((uint64_t)1<<40)+block*512,7,123456));
        }
        assert(!rec_queue_push(&queue,input,0,0,0));
        memset(input,0,sizeof(input)); /* DMA is immediately reused. */
        for(unsigned block=0;block<12;block++) {
            rec_slot_t *slot=rec_queue_peek(&queue);assert(slot);
            assert(rec_get64(slot->bytes+24)==((uint64_t)1<<40)+block*512);
            for(unsigned i=0;i<REC_SETS;i++)for(unsigned c=0;c<3;c++)
                assert(rec_get16(slot->bytes+64+(i*3+c)*2)==((i+c*1000+block)&4095));
            rec_queue_pop(&queue);
        }
        assert(!rec_queue_peek(&queue));assert(queue.high_water==12);
    }
    for(unsigned n=0;n<REC_PAYLOAD/2;n++)rec_put16(encoded+64+n*2,(uint16_t)(n&4095));
    rec_block_info_t i={.type=REC_DATA,.count=512,.payload_size=REC_PAYLOAD,
        .sequence=UINT64_C(0x100000001),.first_index=UINT64_C(0x100000000),.epoch=12};
    rec_encode(encoded,&i);rec_block_info_t decoded;
    assert(rec_validate(encoded,sizeof(encoded),&decoded));
    assert(decoded.sequence==i.sequence&&decoded.first_index==i.first_index);
    for(unsigned length=0;length<REC_RECORD_SIZE;length++)assert(!rec_validate(encoded,length,&decoded));
    for(unsigned byte=0;byte<REC_RECORD_SIZE;byte++) {
        encoded[byte]^=1;assert(!rec_validate(encoded,sizeof(encoded),&decoded));encoded[byte]^=1;
    }
    assert(rec_crc("123456789",9)==0xcbf43926U);
    puts("C queue: wraparound, full/drop, ownership, channel ordering; encoder/parser: every-byte corruption and truncation PASS");
    return 0;
}
