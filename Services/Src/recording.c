#include "recording.h"
#include <stdio.h>
#include <string.h>
#include "acquisition.h"
#include "calibration.h"
#include "ff.h"
#include "rtc_service.h"
#include "session_format.h"
#include "storage.h"
#include "oscilloscope_app.h"

#define DIRECTORY "0:/OSC/SESSIONS"
#define SEGMENT_BYTES (16UL*1024UL*1024UL)
#define CLOSING_RESERVE (4U*REC_RECORD_SIZE)
static rec_queue_t g_queue;
static uint8_t g_work[REC_RECORD_SIZE] __attribute__((aligned(4)));
static uint8_t g_header[REC_HEADER_SIZE] __attribute__((aligned(4)));
static FIL g_file, g_next_file;
static bool g_open, g_next_open, g_processing;
static volatile recording_state_t g_state;
static volatile bool g_accept, g_boundary, g_stop, g_pause;
static volatile uint64_t g_base, g_end, g_dropped;
static uint32_t g_id, g_segment, g_offset, g_error, g_epoch, g_sync_at;
static uint64_t g_sequence, g_expected, g_written, g_unknown, g_errors, g_pauses;
static uint64_t g_start_us, g_pause_us, g_pause_total, g_durable, g_utc;
static uint64_t g_stop_us, g_reported_errors, g_prepared_us;
static uint64_t g_fault_index;
static FIL g_scan;
static bool g_scanning;
static bool g_rtc_valid;
static acquisition_config_t g_settings;
static calibration_path_t g_cal[3];

uint64_t recording_clock_us(void)
{
    static uint32_t last;
    static uint64_t ticks;
    uint32_t irq=__get_PRIMASK();__disable_irq();
    uint32_t now=HAL_GetTick();ticks+=(uint32_t)(now-last);last=now;
    uint64_t value=ticks*1000ULL;__set_PRIMASK(irq);return value;
}
void recording_io_yield(void){oscilloscope_app_io_yield();}
bool recording_busy(void){return g_state!=RECORD_IDLE && g_state!=RECORD_FAILED;}
static void path_for(char *p,size_t size,uint32_t id,uint32_t segment)
{(void)snprintf(p,size,DIRECTORY "/R%08lX_%08lX.OSC",(unsigned long)id,(unsigned long)segment);}
static bool parse_name(const char *p,uint32_t *id,uint32_t *segment)
{
    unsigned long a,b;int n=0;
    if(strlen(p)!=22U||sscanf(p,"R%8lx_%8lx.OSC%n",&a,&b,&n)!=2||n!=22)return false;
    *id=(uint32_t)a;*segment=(uint32_t)b;return true;
}
static bool write_exact(FIL *f,const void *p,UINT n)
{UINT done=0;return f_write(f,p,n,&done)==FR_OK&&done==n;}
static bool read_exact(FIL *f,void *p,UINT n)
{UINT done=0;return f_read(f,p,n,&done)==FR_OK&&done==n;}
static void make_header(uint32_t segment)
{
    memset(g_header,0,sizeof(g_header));
    rec_put32(g_header,REC_FILE_MAGIC);rec_put16(g_header+4,2);rec_put16(g_header+6,512);
    rec_put32(g_header+8,g_id);rec_put32(g_header+12,segment);
    rec_put32(g_header+16,OSC_FIRMWARE_VERSION);rec_put32(g_header+20,g_rtc_valid?1U:0U);
    rec_put32(g_header+28,10000);rec_put32(g_header+32,3);rec_put32(g_header+36,REC_RECORD_SIZE);
    rec_put64(g_header+40,g_utc);rec_put64(g_header+48,g_start_us);rec_put64(g_header+56,g_sequence);
    for(unsigned c=0;c<3;c++){
        uint8_t *p=g_header+64+c*32;p[0]='C';p[1]=(uint8_t)c;p[2]=(uint8_t)(10+c);p[3]=1;
        rec_put32(p+4,(uint32_t)g_cal[c].gain_uv_per_lsb_q16);rec_put32(p+8,(uint32_t)g_cal[c].offset_uv);
        rec_put32(p+12,g_settings.channel[c].microvolts_per_div);rec_put32(p+16,(uint32_t)g_settings.channel[c].vertical_offset_uv);
    }
    rec_put64(g_header+160,g_expected);rec_put32(g_header+168,72000000);rec_put32(g_header+172,7200);
    rec_put32(g_header+176,g_settings.trigger_mode);rec_put32(g_header+180,g_settings.trigger_edge);
    rec_put32(g_header+184,g_settings.trigger_level_raw);rec_put32(g_header+188,g_settings.trigger_position);
    rec_put32(g_header+508,rec_crc(g_header,508));
}

/* Full validation is used before destructive retention. No partial session qualifies. */
static bool validate_segment(uint32_t id,uint32_t seg,uint16_t *end_type,uint64_t *expected)
{
    FIL file;char path[64];uint8_t header[512]={0};rec_block_info_t info;
    path_for(path,sizeof(path),id,seg);*end_type=0;
    if(f_open(&file,path,FA_READ)!=FR_OK)return false;
    bool ok=read_exact(&file,header,512)&&rec_header_valid(header)&&rec_get32(header+8)==id&&rec_get32(header+12)==seg;
    uint64_t sequence=rec_get64(header+56);
    ok=ok&&sequence==*expected;
    while(ok && f_tell(&file)+REC_RECORD_SIZE<=f_size(&file)){
        ok=read_exact(&file,g_work,REC_RECORD_SIZE)&&rec_validate(g_work,REC_RECORD_SIZE,&info)&&
            info.session==id&&info.segment==seg&&info.sequence==sequence++;
        if(ok&&(info.type==REC_SEGMENT_END||info.type==REC_SESSION_END)){
            *end_type=info.type;break;
        }
        recording_io_yield();
    }
    *expected=sequence;
    (void)f_close(&file);return ok&&*end_type!=0;
}
static bool complete_session(uint32_t id,uint32_t *segments)
{
    uint64_t expected=0;
    for(uint32_t s=0;s<65536U;s++){
        uint16_t end;
        if(!validate_segment(id,s,&end,&expected))return false;
        if(end==REC_SESSION_END){*segments=s+1;return true;}
    }
    return false;
}
static bool delete_complete(uint32_t id)
{
    uint32_t count;if(!complete_session(id,&count))return false;
    /* Delete from last to first. Interruption cannot leave a false completed chain. */
    while(count){char path[64];path_for(path,sizeof(path),id,--count);if(f_unlink(path)!=FR_OK)return false;}
    return true;
}
static bool reclaim_oldest(void)
{
    char legacy[96];uint32_t legacy_id;
    /* Legacy captures predate v2 sessions; only validated completed files qualify. */
    if(storage_legacy_oldest(legacy,sizeof(legacy),&legacy_id))return f_unlink(legacy)==FR_OK;
    DIR dir;FILINFO entry;uint32_t oldest=UINT32_MAX;
    if(f_opendir(&dir,DIRECTORY)!=FR_OK)return false;
    while(f_readdir(&dir,&entry)==FR_OK&&entry.fname[0]){
        uint32_t id,seg,count;
        if(!(entry.fattrib&AM_DIR)&&parse_name(entry.fname,&id,&seg)&&seg==0&&id!=g_id&&id<oldest&&complete_session(id,&count))oldest=id;
        recording_io_yield();
    }
    (void)f_closedir(&dir);
    return oldest!=UINT32_MAX&&delete_complete(oldest);
}
static bool reserve_space(void)
{
    FATFS *fs;DWORD free_clusters;
    for(;;){
        if(f_getfree("0:",&free_clusters,&fs)!=FR_OK){g_error=RECORD_IO_ERROR;return false;}
        if((uint64_t)free_clusters*fs->csize*512ULL>=SEGMENT_BYTES+65536U)return true;
        if(!reclaim_oldest()){g_error=RECORD_FULL;return false;}
    }
}
static bool prepare_segment(FIL *file,uint32_t seg)
{
    char path[64];if(!reserve_space())return false;
    path_for(path,sizeof(path),g_id,seg);
    if(f_open(file,path,FA_CREATE_NEW|FA_WRITE|FA_READ)!=FR_OK){g_error=RECORD_IO_ERROR;return false;}
    make_header(seg);
    bool ok=write_exact(file,g_header,512)&&f_lseek(file,SEGMENT_BYTES)==FR_OK&&
        f_tell(file)==SEGMENT_BYTES&&f_sync(file)==FR_OK&&f_lseek(file,512)==FR_OK;
    if(!ok){(void)f_close(file);g_error=RECORD_IO_ERROR;}
    return ok;
}
static void fail(uint32_t error)
{
    uint32_t irq=__get_PRIMASK();__disable_irq();g_accept=false;g_stop=false;g_pause=false;__set_PRIMASK(irq);
    g_error=error;g_state=RECORD_FAILED;++g_errors;
    if(g_open){(void)f_close(&g_file);g_open=false;}
    if(g_next_open){(void)f_close(&g_next_file);g_next_open=false;}
    /* Never unlink a partial file. A restart recovers its valid prefix. */
    if(acquisition_get_state()!=ACQUISITION_RUNNING)(void)acquisition_start();
}
static bool emit(rec_type_t type,uint16_t flags,uint64_t first,uint32_t epoch,
                 uint64_t time,const void *payload,uint32_t size)
{
    if(size)memmove(g_work+64,payload,size);
    rec_block_info_t i={.type=type,.flags=flags,.epoch=epoch,.first_index=first,
        .time_us=time,.sequence=g_sequence,.payload_size=size,.session=g_id,.segment=g_segment};
    rec_encode(g_work,&i);
    if(g_offset+REC_RECORD_SIZE>SEGMENT_BYTES||!write_exact(&g_file,g_work,REC_RECORD_SIZE))return false;
    g_offset+=REC_RECORD_SIZE;++g_sequence;return true;
}
static bool gap_to(uint64_t first,uint32_t epoch,uint64_t time)
{
    if(first<g_expected)return false;
    if((epoch!=g_epoch&&g_epoch!=0)||
       (g_errors>g_reported_errors&&first>=g_fault_index)){
        uint8_t payload[16]={0};rec_put32(payload,RECORD_ADC_ERROR);++g_unknown;
        g_reported_errors=g_errors;
        if(!emit(REC_ERROR,1,g_expected,epoch,time,payload,16)||!emit(REC_GAP,1,g_expected,epoch,time,NULL,0))return false;
    }
    g_epoch=epoch;
    if(first>g_expected){uint8_t payload[16];rec_put64(payload,g_expected);rec_put64(payload+8,first-g_expected);
        if(!emit(REC_GAP,0,g_expected,epoch,time,payload,16))return false;}
    g_expected=first;return true;
}
static bool sync_file(void)
{
    if(f_sync(&g_file)!=FR_OK)return false;
    g_durable=g_sequence*REC_RECORD_SIZE+(uint64_t)(g_segment+1U)*512U;g_sync_at=HAL_GetTick();return true;
}
static bool finish(bool pause)
{
    uint64_t end;uint32_t irq=__get_PRIMASK();__disable_irq();end=g_end;__set_PRIMASK(irq);
    if(!gap_to(end,g_epoch,recording_clock_us()))return false;
    if(g_errors>g_reported_errors) {
        uint8_t error[16]={0};rec_put32(error,RECORD_ADC_ERROR);rec_put64(error+8,g_errors-g_reported_errors);
        if(!emit(REC_ERROR,1,end,g_epoch,recording_clock_us(),error,16)||
           !emit(REC_GAP,1,end,g_epoch,recording_clock_us(),NULL,0))return false;
        ++g_unknown;g_reported_errors=g_errors;
    }
    if(!pause&&g_pause_us)g_pause_total+=g_stop_us-g_pause_us;
    uint8_t summary[112]={0};
    rec_put64(summary,g_written);rec_put64(summary+8,g_dropped);rec_put64(summary+16,g_unknown);
    rec_put64(summary+24,g_pauses);rec_put64(summary+32,g_errors);rec_put64(summary+40,(pause?g_pause_us:g_stop_us)-g_start_us);
    rec_put64(summary+48,(g_written+g_dropped)*100ULL);rec_put64(summary+56,g_sequence+1);
    rec_put32(summary+64,g_segment+1);rec_put32(summary+68,g_error);rec_put64(summary+72,g_pause_total);
    if(!emit(pause?REC_PAUSE:REC_SESSION_END,0,g_expected,g_epoch,recording_clock_us(),summary,sizeof(summary))||!sync_file())return false;
    if(pause){++g_pauses;g_state=RECORD_PAUSED;return true;}
    if(f_truncate(&g_file)!=FR_OK||f_close(&g_file)!=FR_OK)return false;
    g_open=false;
    if(g_next_open){
        (void)f_close(&g_next_file);g_next_open=false;
        /* This file was never activated and contains no records. */
        char path[64];path_for(path,sizeof(path),g_id,g_segment+1);(void)f_unlink(path);
    }
    g_state=g_error?RECORD_FAILED:RECORD_IDLE;
    if(acquisition_get_state()!=ACQUISITION_RUNNING)(void)acquisition_start();
    return true;
}
void recording_init(void){rec_queue_reset(&g_queue);g_state=RECORD_IDLE;}
bool recording_start(void)
{
    if(recording_busy()||g_processing||acquisition_get_state()!=ACQUISITION_RUNNING)return false;
    const acquisition_config_t *c=acquisition_get_config();
    if(c->requested_sample_rate_hz!=10000U)return false;
    for(unsigned i=0;i<3;i++)if(!c->channel[i].enabled||c->channel[i].route!=OSC_INPUT_HIGH_RANGE)return false;
    g_settings=*c;for(unsigned i=0;i<3;i++)g_cal[i]=*calibration_get(i,OSC_INPUT_HIGH_RANGE);
    g_state=RECORD_PREPARING;return true;
}
bool recording_pause(void)
{
    if(g_state!=RECORD_ACTIVE)return false;
    g_pause=true;g_state=RECORD_PAUSING;return true;
}
bool recording_resume(void)
{
    if(g_state!=RECORD_PAUSED||g_processing)return false;
    /* Deferred: the process loop owns the filesystem. */
    g_state=RECORD_ARMED;return true;
}
bool recording_stop(void)
{
    if(g_state!=RECORD_ACTIVE&&g_state!=RECORD_PAUSED&&g_state!=RECORD_ARMED&&g_state!=RECORD_PAUSING)return false;
    g_stop=true;g_state=RECORD_FINALIZING;
    if(acquisition_get_state()!=ACQUISITION_RUNNING||!g_accept){g_boundary=true;g_stop_us=recording_clock_us();}
    return true;
}
bool recording_ingest(const uint16_t *samples,uint64_t index,uint32_t epoch,uint64_t time)
{
    if(g_state==RECORD_ARMED&&!g_pause_us){
        g_base=index+REC_SETS;g_end=0;g_start_us=time+51200ULL;g_state=RECORD_ACTIVE;g_accept=true;
        return false; /* First accepted data begins after this boundary. */
    }
    if(!g_accept)return false;
    uint64_t first=index-g_base;
    if(!rec_queue_push(&g_queue,samples,first,epoch,time))g_dropped+=REC_SETS;
    g_end=first+REC_SETS;
    if(g_stop||g_pause){bool pause=g_pause&&!g_stop;g_accept=false;g_boundary=true;g_pause=false;
        if(pause)g_pause_us=recording_clock_us();else g_stop_us=recording_clock_us();
        return pause;}
    return false;
}
void recording_adc_error(void)
{
    if(recording_busy()){
        if(g_errors==g_reported_errors)g_fault_index=g_end;
        ++g_errors;
    }
}
void recording_status(recording_status_t *s)
{
    uint32_t irq=__get_PRIMASK();__disable_irq();
    *s=(recording_status_t){g_state,g_error,g_id,g_segment,g_queue.high_water,rec_queue_count(&g_queue),
        g_written,g_dropped,g_unknown,g_sequence,g_durable,0};__set_PRIMASK(irq);
    uint64_t end=g_stop_us?g_stop_us:recording_clock_us();
    s->elapsed_us=g_start_us&&end>g_start_us?end-g_start_us:0;
}
void recording_process(void)
{
    if(g_processing)return;
    g_processing=true;
    if(g_state==RECORD_PREPARING){
        g_error=0;g_sequence=0;g_expected=0;g_written=0;g_dropped=0;g_unknown=0;g_errors=0;
        g_pauses=0;g_pause_total=0;g_pause_us=0;g_durable=0;g_segment=0;g_epoch=0;g_boundary=false;g_stop=false;g_pause=false;g_accept=false;
        rec_queue_reset(&g_queue);
        g_end=0;g_stop_us=0;g_reported_errors=0;
        if(g_scanning){(void)f_close(&g_scan);g_scanning=false;}
        if(!storage_mount()){fail(RECORD_NO_CARD);goto out;}
        DIR dir;FILINFO entry;uint32_t max=0;
        if(f_opendir(&dir,DIRECTORY)!=FR_OK){fail(RECORD_IO_ERROR);goto out;}
        while(f_readdir(&dir,&entry)==FR_OK&&entry.fname[0]){uint32_t id,seg;if(parse_name(entry.fname,&id,&seg)&&id>max)max=id;}
        (void)f_closedir(&dir);g_id=max+1;
        if(g_id==0){fail(RECORD_FULL);goto out;}
        g_start_us=recording_clock_us();g_prepared_us=g_start_us;g_rtc_valid=rtc_service_get_utc(&g_utc);
        if(!prepare_segment(&g_file,0)){fail(g_error);goto out;}
        g_open=true;g_offset=512;g_sync_at=HAL_GetTick();g_state=RECORD_ARMED;
    }
    if(g_state==RECORD_ARMED&&g_pause_us){
        uint64_t paused_at=g_pause_us;
        g_pause_us=0;g_boundary=false;g_accept=true;g_state=RECORD_ACTIVE;
        /* Expected next epoch is intentional, not an ADC error. */
        g_epoch=0;
        if(!acquisition_resume()){fail(RECORD_ADC_ERROR);goto out;}
        uint8_t p[8];uint64_t now=recording_clock_us();uint64_t duration=now-paused_at;
        rec_put64(p,duration);g_pause_total+=duration;
        /* Acquisition is already running: SD latency belongs to recording,
           not the intentional pause. ISR-owned slots retain resumed data. */
        if(!emit(REC_RESUME,0,g_expected,g_epoch,now,p,8)){fail(RECORD_IO_ERROR);goto out;}
    }
    if(!g_open||g_state==RECORD_PAUSED)goto out;
    if(acquisition_get_state()==ACQUISITION_ERROR&&g_accept){
        /* A failed restart must not leave an apparently active recording hung. */
        g_accept=false;g_error=RECORD_ADC_ERROR;g_stop=true;g_boundary=true;
        g_stop_us=recording_clock_us();g_state=RECORD_FINALIZING;
    }
    if(!g_next_open&&g_offset>SEGMENT_BYTES-1024U*1024U&&!g_stop){
        if(!prepare_segment(&g_next_file,g_segment+1)){
            if(g_error==RECORD_FULL){(void)recording_stop();}
            else {fail(g_error);goto out;}
        }else g_next_open=true;
    }
    rec_slot_t *slot=rec_queue_peek(&g_queue);
    if(slot){
        if(g_sequence==0){
            if(g_rtc_valid)g_utc+=(g_start_us-g_prepared_us)/1000000ULL;
            make_header(0);
            if(f_lseek(&g_file,0)!=FR_OK||!write_exact(&g_file,g_header,512)){fail(RECORD_IO_ERROR);goto out;}
        }
        if(g_offset+8U*REC_RECORD_SIZE>SEGMENT_BYTES){
            if(!g_next_open){fail(RECORD_FULL);goto out;}
            if(!emit(REC_SEGMENT_END,0,g_expected,g_epoch,recording_clock_us(),NULL,0)||!sync_file()||
               f_truncate(&g_file)!=FR_OK||f_close(&g_file)!=FR_OK){fail(RECORD_IO_ERROR);goto out;}
            g_file=g_next_file;g_next_open=false;++g_segment;g_offset=512;make_header(g_segment);
            if(f_lseek(&g_file,0)!=FR_OK||!write_exact(&g_file,g_header,512)||!sync_file()){fail(RECORD_IO_ERROR);goto out;}
        }
        uint8_t *b=slot->bytes;uint64_t first=rec_get64(b+24),time=rec_get64(b+40);uint32_t epoch=rec_get32(b+32);
        if(!gap_to(first,epoch,time)){fail(RECORD_IO_ERROR);goto out;}
        rec_block_info_t i={.type=REC_DATA,.count=REC_SETS,.epoch=epoch,.payload_size=REC_PAYLOAD,
            .sequence=g_sequence,.first_index=first,.time_us=time,.session=g_id,.segment=g_segment};
        rec_encode(b,&i);
        if(!write_exact(&g_file,b,REC_RECORD_SIZE)){fail(RECORD_IO_ERROR);goto out;}
        g_offset+=REC_RECORD_SIZE;++g_sequence;g_written+=REC_SETS;g_expected=first+REC_SETS;rec_queue_pop(&g_queue);
    }
    if(g_boundary&&!rec_queue_count(&g_queue)){
        if(!finish(!g_stop)){fail(RECORD_IO_ERROR);goto out;}g_boundary=false;g_stop=false;
    }else if(g_open&&(uint32_t)(HAL_GetTick()-g_sync_at)>=1000U){if(!sync_file())fail(RECORD_IO_ERROR);}
out:g_processing=false;
}

/* Recovery is incremental and demand-driven. A list request starts a scan;
   later calls advance by a single record, preserving live UI responsiveness. */
static uint32_t g_scan_cursor, g_scan_valid, g_scan_id, g_scan_segment;
static uint64_t g_scan_sequence, g_scan_sets;
static uint8_t g_scan_info[64];
int recording_list(uint32_t cursor,uint8_t *info,uint32_t *next)
{
    if(recording_busy()||g_processing)return STORAGE_RESULT_BUSY;
    if(!g_scanning||cursor!=g_scan_cursor){
        if(g_scanning){(void)f_close(&g_scan);g_scanning=false;}
        DIR d;FILINFO e;uint32_t n=0;bool found=false;char path[64];
        if(f_opendir(&d,DIRECTORY)!=FR_OK)return STORAGE_RESULT_NO_CARD;
        while(f_readdir(&d,&e)==FR_OK&&e.fname[0]){
            if(parse_name(e.fname,&g_scan_id,&g_scan_segment)&&n++==cursor){found=true;break;}
        }
        (void)f_closedir(&d);if(!found)return STORAGE_RESULT_NOT_FOUND;
        path_for(path,sizeof(path),g_scan_id,g_scan_segment);
        if(f_open(&g_scan,path,FA_READ)!=FR_OK)return STORAGE_RESULT_IO_ERROR;
        if(!read_exact(&g_scan,g_header,512)||!rec_header_valid(g_header)){(void)f_close(&g_scan);return STORAGE_RESULT_INVALID;}
        memset(g_scan_info,0,64);rec_put32(g_scan_info,g_scan_id);rec_put32(g_scan_info+4,g_scan_segment);
        rec_put64(g_scan_info+24,rec_get64(g_header+40));
        g_scan_sequence=rec_get64(g_header+56);g_scan_sets=0;g_scan_valid=512;g_scan_cursor=cursor;g_scanning=true;
    }
    rec_block_info_t block;uint32_t flags=1;
    bool valid=read_exact(&g_scan,g_work,REC_RECORD_SIZE)&&rec_validate(g_work,REC_RECORD_SIZE,&block)&&
        block.session==g_scan_id&&block.segment==g_scan_segment&&block.sequence==g_scan_sequence;
    if(valid){g_scan_valid+=REC_RECORD_SIZE;++g_scan_sequence;if(block.type==REC_DATA)g_scan_sets+=block.count;
        if(block.type!=REC_SESSION_END&&block.type!=REC_SEGMENT_END)return STORAGE_RESULT_BUSY;
        flags=block.type==REC_SESSION_END?2U:4U;
    }
    (void)f_close(&g_scan);g_scanning=false;rec_put32(g_scan_info+8,g_scan_valid);rec_put32(g_scan_info+12,flags);
    rec_put64(g_scan_info+16,g_scan_sets);memcpy(info,g_scan_info,64);*next=cursor+1;return STORAGE_RESULT_OK;
}
int recording_read(uint32_t id,uint32_t seg,uint32_t offset,uint8_t *data,uint32_t length,uint32_t *actual)
{
    if(recording_busy()||g_processing)return STORAGE_RESULT_BUSY;
    char path[64];FIL f;UINT got=0;path_for(path,sizeof(path),id,seg);
    if(f_open(&f,path,FA_READ)!=FR_OK)return STORAGE_RESULT_NOT_FOUND;
    FRESULT r=f_lseek(&f,offset);if(r==FR_OK)r=f_read(&f,data,length,&got);
    (void)f_close(&f);*actual=got;return r==FR_OK?STORAGE_RESULT_OK:STORAGE_RESULT_IO_ERROR;
}
int recording_delete(uint32_t id)
{
    if(recording_busy()||g_processing||g_scanning)return STORAGE_RESULT_BUSY;
    return delete_complete(id)?STORAGE_RESULT_OK:STORAGE_RESULT_INVALID;
}
