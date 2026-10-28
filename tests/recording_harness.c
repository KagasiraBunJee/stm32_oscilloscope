/* Executes the actual recording service against a deterministic in-memory disk.
 * This does NOT model SD electrical timing, interrupt latency, or FAT corruption. */
#include "recording_stubs/platform.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../Services/Src/recording.c"
typedef struct {char name[64];uint8_t *data;uint32_t size,capacity;bool exists;} mock_file;
static mock_file files[32];
static uint32_t tick;static int adc_state=ACQUISITION_RUNNING;
static bool card=true,write_error=false,full=false;
static unsigned inject_blocks;
static uint64_t index_sets;
static uint32_t epoch=1;
static uint16_t samples[1536];
static acquisition_config_t config;
static calibration_path_t calibration={301037074,0};
uint32_t __get_PRIMASK(void){return 0;}void __disable_irq(void){}void __set_PRIMASK(uint32_t v){(void)v;}
uint32_t HAL_GetTick(void){return tick;}
int acquisition_get_state(void){return adc_state;}
bool acquisition_start(void){adc_state=ACQUISITION_RUNNING;epoch++;return true;}
bool acquisition_resume(void){return acquisition_start();}
const acquisition_config_t *acquisition_get_config(void){return &config;}
const calibration_path_t *calibration_get(unsigned c,int r){(void)c;(void)r;return &calibration;}
bool rtc_service_get_utc(uint64_t *u){*u=0;return false;}
bool storage_mount(void){return card;}
bool storage_legacy_oldest(char *p,size_t n,uint32_t *id){(void)p;(void)n;(void)id;return false;}
void oscilloscope_app_io_yield(void){}
static void block(void) {
    for(unsigned i=0;i<512;i++)for(unsigned c=0;c<3;c++)samples[3*i+c]=(uint16_t)((index_sets+i+c*1000)&4095);
    uint64_t time=(uint64_t)tick*1000;tick+=52;
    if(recording_ingest(samples,index_sets,epoch,time))adc_state=ACQUISITION_PAUSED;
    index_sets+=512;
}
FRESULT f_open(FIL *f,const char *p,unsigned flags){
    if(!card)return FR_ERROR;
    const char *n=strrchr(p,'/');n=n?n+1:p;int free_slot=-1;
    for(int i=0;i<32;i++){if(!files[i].exists){if(free_slot<0)free_slot=i;continue;}
        if(!strcmp(files[i].name,n)){if(flags&FA_CREATE_NEW)return FR_ERROR;*f=(FIL){i,0};return FR_OK;}}
    if(!(flags&FA_CREATE_NEW)||free_slot<0)return FR_ERROR;
    mock_file *m=&files[free_slot];m->exists=true;m->size=0;snprintf(m->name,64,"%s",n);*f=(FIL){free_slot,0};return FR_OK;
}
FRESULT f_close(FIL *f){(void)f;return FR_OK;}
FRESULT f_write(FIL *f,const void *p,UINT n,UINT *done){
    *done=0;if(!card||write_error)return FR_ERROR;
    while(inject_blocks){--inject_blocks;block();}
    mock_file *m=&files[f->file];uint32_t end=f->pos+n;
    if(end>m->capacity){uint32_t capacity=(end+65535)&~65535U;void *newdata=realloc(m->data,capacity);assert(newdata);m->data=newdata;memset(m->data+m->capacity,0,capacity-m->capacity);m->capacity=capacity;}
    memcpy(m->data+f->pos,p,n);f->pos=end;if(end>m->size)m->size=end;*done=n;return FR_OK;
}
FRESULT f_read(FIL *f,void *p,UINT n,UINT *done){
    if(!card)return FR_ERROR;
    mock_file *m=&files[f->file];
    if(f->pos>m->size){*done=0;return FR_OK;}if(n>m->size-f->pos)n=m->size-f->pos;
    memset(p,0,n);uint32_t available=f->pos<m->capacity?m->capacity-f->pos:0;
    if(available>n)available=n;
    if(available)memcpy(p,m->data+f->pos,available);
    f->pos+=n;*done=n;return FR_OK;
}
FRESULT f_lseek(FIL *f,uint32_t pos){f->pos=pos;if(files[f->file].size<pos)files[f->file].size=pos;return FR_OK;}
uint32_t f_tell(FIL *f){return f->pos;}uint32_t f_size(FIL *f){return files[f->file].size;}
FRESULT f_sync(FIL *f){(void)f;return card&&!write_error?FR_OK:FR_ERROR;}
FRESULT f_truncate(FIL *f){files[f->file].size=f->pos;return FR_OK;}
FRESULT f_unlink(const char *p){const char *n=strrchr(p,'/');n=n?n+1:p;for(unsigned i=0;i<32;i++)if(files[i].exists&&!strcmp(files[i].name,n)){files[i].exists=false;full=false;return FR_OK;}return FR_ERROR;}
FRESULT f_getfree(const char *p,DWORD *n,FATFS **fs){static FATFS disk={8};(void)p;*n=full?0:100000;*fs=&disk;return card?FR_OK:FR_ERROR;}
FRESULT f_opendir(DIR *d,const char *p){(void)p;d->index=0;return card?FR_OK:FR_ERROR;}
FRESULT f_readdir(DIR *d,FILINFO *e){memset(e,0,sizeof(*e));while(d->index<32){mock_file *m=&files[d->index++];if(m->exists){strcpy(e->fname,m->name);return FR_OK;}}return FR_OK;}
FRESULT f_closedir(DIR *d){(void)d;return FR_OK;}
static void drain(void){for(unsigned i=0;i<100&&rec_queue_count(&g_queue);i++)recording_process();}
static void start(void){assert(recording_start());assert(g_state==RECORD_PREPARING);recording_process();assert(g_state==RECORD_ARMED);block();assert(g_state==RECORD_ACTIVE);assert(!rec_queue_count(&g_queue));}
static void stop(void){assert(recording_stop());if(g_accept)block();drain();recording_process();assert(!recording_busy());assert(adc_state==ACQUISITION_RUNNING);}
static void check_session(uint32_t id){uint32_t count=0;assert(complete_session(id,&count));assert(count>=1);}
int main(int argc,char **argv){
    config.requested_sample_rate_hz=10000;for(unsigned c=0;c<3;c++){config.channel[c].enabled=true;config.channel[c].route=1;config.channel[c].microvolts_per_div=4000000;}
    recording_init();card=false;assert(recording_start());recording_process();assert(g_state==RECORD_FAILED&&adc_state==ACQUISITION_RUNNING);card=true;
    start();assert(!recording_start());
    block();recording_process();assert(g_written==512);
    inject_blocks=20;block();recording_process();assert(g_dropped>0);drain();block();recording_process();
    assert(recording_pause());block();drain();recording_process();assert(g_state==RECORD_PAUSED&&adc_state==ACQUISITION_PAUSED);
    uint8_t info[64];uint32_t next;assert(recording_list(0,info,&next)==STORAGE_RESULT_BUSY);
    tick+=250;assert(recording_resume());recording_process();block();recording_process();
    recording_adc_error();epoch++;block();recording_process();stop();check_session(g_id);
    assert(g_unknown==1&&g_pauses==1&&g_written+g_dropped==g_end);
    if(argc>1){FILE *out=fopen(argv[1],"wb");assert(out);assert(fwrite(files[0].data,1,files[0].size,out)==files[0].size);fclose(out);}
    start();block();recording_process();assert(recording_pause());block();drain();recording_process();tick+=500;stop();check_session(g_id);
    start();block();recording_process();write_error=true;block();recording_process();assert(g_state==RECORD_FAILED&&adc_state==ACQUISITION_RUNNING);write_error=false;
    uint32_t ignored;assert(!complete_session(g_id,&ignored));
    /* Cross a real 16 MiB rollover, not a forced offset. */
    start();for(unsigned i=0;i<4700;i++){block();recording_process();}stop();assert(g_segment==1);check_session(g_id);
    full=true;start();/* oldest completed session was reclaimed, partial retained */
    assert(files[2].exists);stop();
    /* All remaining completed sessions are deliberately hidden from retention. */
    for(unsigned i=0;i<32;i++)if(files[i].exists)files[i].data[0]^=1;
    full=true;assert(recording_start());recording_process();assert(g_state==RECORD_FAILED&&g_error==RECORD_FULL);
    full=false;start();block();recording_process();recording_adc_error();adc_state=ACQUISITION_ERROR;
    recording_process();assert(g_state==RECORD_FAILED&&g_error==RECORD_ADC_ERROR);check_session(g_id);
    /* No eligible older sessions: exhaust space during recording, not just Start. */
    files[g_file.file].data[0]^=1;start();full=true;
    for(unsigned i=0;i<4700&&recording_busy();i++){block();recording_process();}
    assert(g_state==RECORD_FAILED&&g_error==RECORD_FULL);check_session(g_id);
    for(unsigned i=0;i<32;i++)free(files[i].data);
    puts("C recording service: boundaries, delay overflow, pause/resume, unknown loss, SD error, repeated sessions, rollover, retention/full PASS");return 0;
}
