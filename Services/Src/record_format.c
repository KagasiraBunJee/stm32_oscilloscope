#include "record_format.h"
#include <string.h>
void rec_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
void rec_put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
void rec_put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
uint16_t rec_get16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
uint32_t rec_get32(const uint8_t *p){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
uint64_t rec_get64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
uint32_t rec_crc(const void *data,size_t size){
    const uint8_t *p=data;uint32_t crc=0xffffffffU;
    while(size--){crc^=*p++;for(unsigned i=0;i<8;i++)crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U)));}
    return ~crc;
}
void rec_encode(uint8_t *b,const rec_block_info_t *i){
    if(!b || !i || i->payload_size>REC_PAYLOAD)return;
    /* Payload has already been placed at byte 64. */
    memset(b,0,64);memset(b+64+i->payload_size,0,REC_RECORD_SIZE-64-i->payload_size);
    rec_put32(b,REC_BLOCK_MAGIC);rec_put16(b+4,i->type);rec_put16(b+6,i->flags);
    rec_put32(b+8,REC_RECORD_SIZE);rec_put32(b+12,i->count);
    rec_put64(b+16,i->sequence);rec_put64(b+24,i->first_index);
    rec_put32(b+32,i->epoch);rec_put32(b+36,i->payload_size);rec_put64(b+40,i->time_us);
    rec_put32(b+48,i->session);rec_put32(b+52,i->segment);
    rec_put32(b+56,rec_crc(b+64,i->payload_size));rec_put32(b+60,rec_crc(b,60));
    rec_put32(b+REC_RECORD_SIZE-16,0x32444e45U);
    rec_put64(b+REC_RECORD_SIZE-12,i->sequence);
    rec_put32(b+REC_RECORD_SIZE-4,rec_crc(b,REC_RECORD_SIZE-4));
}
bool rec_validate(const uint8_t *b,size_t size,rec_block_info_t *i){
    if(!b||!i||size!=REC_RECORD_SIZE||rec_get32(b)!=REC_BLOCK_MAGIC||
       rec_get32(b+8)!=REC_RECORD_SIZE||rec_get32(b+60)!=rec_crc(b,60))return false;
    i->type=rec_get16(b+4);i->flags=rec_get16(b+6);i->count=rec_get32(b+12);
    i->sequence=rec_get64(b+16);i->first_index=rec_get64(b+24);
    i->epoch=rec_get32(b+32);i->payload_size=rec_get32(b+36);i->time_us=rec_get64(b+40);
    i->session=rec_get32(b+48);i->segment=rec_get32(b+52);
    if(i->type<REC_DATA||i->type>REC_SESSION_END||i->payload_size>REC_PAYLOAD)return false;
    if(i->type==REC_DATA&&(i->count==0||i->count>REC_SETS||i->payload_size!=i->count*6U))return false;
    if(i->type!=REC_DATA&&i->count!=0)return false;
    if(i->type==REC_GAP&&i->payload_size!=((i->flags&1U)?0U:16U))return false;
    if((i->type==REC_PAUSE||i->type==REC_SESSION_END)&&i->payload_size!=112U)return false;
    if(i->type==REC_RESUME&&i->payload_size!=8U)return false;
    if(i->type==REC_SEGMENT_END&&i->payload_size!=0U)return false;
    if(i->type==REC_ERROR&&i->payload_size!=16U)return false;
    if(i->type==REC_DATA)for(uint32_t n=0;n<i->payload_size;n+=2)if(rec_get16(b+64+n)>4095U)return false;
    return rec_get32(b+56)==rec_crc(b+64,i->payload_size)&&
        rec_get32(b+REC_RECORD_SIZE-16)==0x32444e45U&&
        rec_get64(b+REC_RECORD_SIZE-12)==i->sequence&&
        rec_get32(b+REC_RECORD_SIZE-4)==rec_crc(b,REC_RECORD_SIZE-4);
}
bool rec_header_valid(const uint8_t *h){return rec_get32(h)==REC_FILE_MAGIC&&rec_get16(h+4)==2&&
    rec_get16(h+6)==REC_HEADER_SIZE&&rec_get32(h+28)==10000&&rec_get32(h+32)==3&&
    rec_get32(h+36)==REC_RECORD_SIZE&&h[64]=='C'&&h[65]==0&&h[66]==10&&h[67]==1&&
    h[96]=='C'&&h[97]==1&&h[98]==11&&h[99]==1&&h[128]=='C'&&h[129]==2&&h[130]==12&&h[131]==1&&
    rec_get32(h+508)==rec_crc(h,508);}
void rec_queue_reset(rec_queue_t *q){q->producer=0;q->consumer=0;q->high_water=0;q->producer_slot=0;q->consumer_slot=0;}
uint32_t rec_queue_count(const rec_queue_t *q){return __atomic_load_n(&q->producer,__ATOMIC_ACQUIRE)-__atomic_load_n(&q->consumer,__ATOMIC_ACQUIRE);}
bool rec_queue_push(rec_queue_t *q,const uint16_t *samples,uint64_t index,uint32_t epoch,uint64_t time_us){
    uint32_t p=q->producer,c=__atomic_load_n(&q->consumer,__ATOMIC_ACQUIRE);
    if(p-c>=REC_QUEUE_SLOTS)return false;
    uint8_t *b=q->slots[q->producer_slot].bytes;
    memcpy(b+64,samples,REC_PAYLOAD);rec_put64(b+24,index);rec_put32(b+32,epoch);rec_put64(b+40,time_us);
    if(p-c+1>q->high_water)q->high_water=p-c+1;
    q->producer_slot=(q->producer_slot+1)%REC_QUEUE_SLOTS;
    __atomic_store_n(&q->producer,p+1,__ATOMIC_RELEASE);return true;
}
rec_slot_t *rec_queue_peek(rec_queue_t *q){return rec_queue_count(q)?&q->slots[q->consumer_slot]:NULL;}
void rec_queue_pop(rec_queue_t *q){if(!rec_queue_count(q))return;q->consumer_slot=(q->consumer_slot+1)%REC_QUEUE_SLOTS;__atomic_store_n(&q->consumer,q->consumer+1,__ATOMIC_RELEASE);}
