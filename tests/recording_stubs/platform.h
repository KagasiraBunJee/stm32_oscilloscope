#ifndef TEST_PLATFORM_H
#define TEST_PLATFORM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OSC_INPUT_HIGH_RANGE 1
#define OSC_FIRMWARE_VERSION 0x20000U
typedef struct { bool enabled;int route;uint32_t microvolts_per_div;int32_t vertical_offset_uv;} channel_t;
typedef struct {uint32_t requested_sample_rate_hz;channel_t channel[3];uint32_t trigger_mode,trigger_edge,trigger_level_raw,trigger_position;} acquisition_config_t;
typedef struct {int32_t gain_uv_per_lsb_q16,offset_uv;} calibration_path_t;
enum {ACQUISITION_RUNNING=1,ACQUISITION_PAUSED=2,ACQUISITION_ERROR=4};
uint32_t __get_PRIMASK(void);void __disable_irq(void);void __set_PRIMASK(uint32_t v);
uint32_t HAL_GetTick(void);
int acquisition_get_state(void);bool acquisition_start(void);bool acquisition_resume(void);
const acquisition_config_t *acquisition_get_config(void);
const calibration_path_t *calibration_get(unsigned c,int route);
bool rtc_service_get_utc(uint64_t *utc);
bool storage_mount(void);void oscilloscope_app_io_yield(void);
bool storage_legacy_oldest(char *p,size_t n,uint32_t *id);
enum {STORAGE_RESULT_OK,STORAGE_RESULT_BUSY,STORAGE_RESULT_NO_CARD,STORAGE_RESULT_NOT_FOUND,STORAGE_RESULT_IO_ERROR,STORAGE_RESULT_INVALID};
typedef unsigned UINT;typedef uint32_t DWORD;typedef int FRESULT;
enum {FR_OK,FR_ERROR,FA_READ=1,FA_WRITE=2,FA_CREATE_NEW=4,AM_DIR=0x10};
typedef struct {int file;uint32_t pos;} FIL;
typedef struct {unsigned index;} DIR;
typedef struct {char fname[64];unsigned fattrib;} FILINFO;
typedef struct {uint32_t csize;} FATFS;
FRESULT f_open(FIL *,const char *,unsigned);FRESULT f_close(FIL *);
FRESULT f_write(FIL *,const void *,UINT,UINT *);FRESULT f_read(FIL *,void *,UINT,UINT *);
FRESULT f_lseek(FIL *,uint32_t);uint32_t f_tell(FIL *);uint32_t f_size(FIL *);
FRESULT f_sync(FIL *);FRESULT f_truncate(FIL *);FRESULT f_unlink(const char *);
FRESULT f_getfree(const char *,DWORD *,FATFS **);
FRESULT f_opendir(DIR *,const char *);FRESULT f_readdir(DIR *,FILINFO *);FRESULT f_closedir(DIR *);
#endif
