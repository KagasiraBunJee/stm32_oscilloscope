#include "storage.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "acquisition.h"
#include "ff.h"
#include "diskio.h"
#include "rtc_service.h"
#include "recording.h"

#define STORAGE_ROOT "0:/OSC"
#define STORAGE_DIRECTORY "0:/OSC/SESSIONS"
#define STORAGE_CHUNK_SIZE 1024U

static FATFS g_filesystem;
static FIL g_save_file;
static storage_state_t g_state;
static bool g_mounted;
static uint32_t g_next_session_id;
static uint32_t g_save_offset;
static uint8_t g_progress;
static const osc_capture_t *g_pending_capture;
static osc_stop_reason_t g_pending_reason;
static osc_session_header_v1_t g_header;
static char g_final_path[96];
static char g_temp_path[96];
static uint8_t g_io_buffer[STORAGE_CHUNK_SIZE] __attribute__((aligned(4)));

static bool has_osc_extension(const char *name)
{
    size_t length;
    if (name == NULL) return false;
    length = strlen(name);
    return (length >= 4U) && (name[length - 4U] == '.') &&
           (toupper((unsigned char)name[length - 3U]) == 'O') &&
           (toupper((unsigned char)name[length - 2U]) == 'S') &&
           (toupper((unsigned char)name[length - 1U]) == 'C');
}

static const char *file_info_name(const FILINFO *info)
{
    return info->fname;
}

static bool read_header_path(const char *path, osc_session_header_v1_t *header,
                             uint32_t *size)
{
    FIL file;
    UINT read = 0U;
    uint8_t buffer[256];
    uint32_t remaining;
    uint32_t crc = 0U;
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK) return false;
    *size = (uint32_t)f_size(&file);
    result = f_read(&file, header, sizeof(*header), &read);
    if ((result != FR_OK) || (read != sizeof(*header)) ||
        !session_validate_header(header, *size)) {
        (void)f_close(&file);
        return false;
    }
    remaining = header->payload_size;
    while (remaining != 0U) {
        UINT request = (remaining > sizeof(buffer)) ? sizeof(buffer) : (UINT)remaining;
        result = f_read(&file, buffer, request, &read);
        if ((result != FR_OK) || (read != request)) {
            (void)f_close(&file);
            return false;
        }
        crc = session_crc32(buffer, read, crc);
        remaining -= read;
        recording_io_yield();
    }
    (void)f_close(&file);
    return crc == header->payload_crc32;
}

static bool mount_card(void)
{
    uint32_t maximum_id = 0U;
    FILINFO directory_info;
    FRESULT result = f_mount(&g_filesystem, "0:", 1U);
    /*
     * Small SD cards (notably 2 GB cards) are commonly formatted as FAT16.
     * FatFS supports FAT16 and FAT32 with the same file API, so accept both
     * on-media formats.  Formatting remains a host-side operation.
     */
    if ((result != FR_OK) ||
        ((g_filesystem.fs_type != FS_FAT16) &&
         (g_filesystem.fs_type != FS_FAT32))) {
        (void)f_mount(NULL, "0:", 0U);
        g_mounted = false;
        return false;
    }
    if ((f_mkdir(STORAGE_ROOT) != FR_OK) &&
        ((f_stat(STORAGE_ROOT, &directory_info) != FR_OK) ||
         ((directory_info.fattrib & AM_DIR) == 0U))) goto mount_failed;
    if ((f_mkdir(STORAGE_DIRECTORY) != FR_OK) &&
        ((f_stat(STORAGE_DIRECTORY, &directory_info) != FR_OK) ||
         ((directory_info.fattrib & AM_DIR) == 0U))) goto mount_failed;
    /* Interrupted recordings are retained. v2 recovery is demand-driven. */
    g_next_session_id = maximum_id + 1U;
    if (g_next_session_id == 0U) g_next_session_id = 1U;
    g_mounted = true;
    return true;

mount_failed:
    (void)f_mount(NULL, "0:", 0U);
    g_mounted = false;
    return false;
}

bool storage_legacy_oldest(char *path, size_t path_size, uint32_t *session_id)
{
    DIR directory;
    FILINFO info;
    FRESULT result;
    uint32_t oldest = UINT32_MAX;
    char oldest_path[96] = {0};

    memset(&info, 0, sizeof(info));
    if (f_opendir(&directory, STORAGE_DIRECTORY) != FR_OK) return false;
    for (;;) {
        char candidate[96];
        osc_session_header_v1_t header;
        uint32_t size;
        result = f_readdir(&directory, &info);
        if ((result != FR_OK) || (info.fname[0] == '\0')) break;
        if ((info.fattrib & AM_DIR) != 0U || !has_osc_extension(file_info_name(&info))) {
            continue;
        }
        (void)snprintf(candidate, sizeof(candidate), "%s/%s",
                       STORAGE_DIRECTORY, file_info_name(&info));
        if (read_header_path(candidate, &header, &size) &&
            header.session_id < oldest) {
            oldest = header.session_id;
            (void)snprintf(oldest_path, sizeof(oldest_path), "%s", candidate);
        }
    }
    (void)f_closedir(&directory);
    if ((result != FR_OK) || (oldest == UINT32_MAX)) return false;
    (void)snprintf(path, path_size, "%s", oldest_path);
    *session_id = oldest;
    return true;
}

static bool ensure_free_space(uint32_t required)
{
    FATFS *filesystem;
    DWORD free_clusters;
    for (;;) {
        uint64_t free_bytes;
        char oldest_path[96];
        uint32_t oldest_id;
        if (f_getfree("0:", &free_clusters, &filesystem) != FR_OK) return false;
        free_bytes = (uint64_t)free_clusters * filesystem->csize * 512ULL;
        if (free_bytes >= (uint64_t)required + filesystem->csize * 512ULL) {
            return true;
        }
        if (!storage_legacy_oldest(oldest_path, sizeof(oldest_path), &oldest_id) ||
            (f_unlink(oldest_path) != FR_OK)) return false;
    }
}

static void save_failed(storage_state_t state)
{
    (void)f_close(&g_save_file);
    if (g_mounted && g_temp_path[0] != '\0') (void)f_unlink(g_temp_path);
    g_state = state;
    g_progress = 0U;
    if ((state == STORAGE_ERROR) || (state == STORAGE_NO_CARD)) {
        (void)f_mount(NULL, "0:", 0U);
        g_mounted = false;
    }
}

static storage_result_t begin_save(void)
{
    UINT written;
    uint64_t timestamp = 0U;
    bool rtc_valid;
    char filename[40];

    if (g_pending_capture == NULL) return STORAGE_RESULT_INVALID;
    if (!g_mounted && !mount_card()) {
        g_state = STORAGE_UNSAVED;
        return STORAGE_RESULT_NO_CARD;
    }
    rtc_valid = rtc_service_get_utc(&timestamp);
    if (!session_build_header(&g_header, g_pending_capture, g_next_session_id,
                              timestamp, rtc_valid, g_pending_reason)) {
        g_state = STORAGE_ERROR;
        return STORAGE_RESULT_INVALID;
    }
    if (!ensure_free_space(g_header.total_length)) {
        g_state = STORAGE_FULL;
        return STORAGE_RESULT_FULL;
    }
    if (!rtc_service_format_filename(filename, sizeof(filename),
                                     g_next_session_id)) {
        (void)snprintf(filename, sizeof(filename), "SESSION_%08lu.OSC",
                       (unsigned long)g_next_session_id);
    }
    (void)snprintf(g_final_path, sizeof(g_final_path), "%s/%s",
                   STORAGE_DIRECTORY, filename);
    (void)snprintf(g_temp_path, sizeof(g_temp_path),
                   "%s/SESSION_%08lu.TMP", STORAGE_DIRECTORY,
                   (unsigned long)g_next_session_id);
    (void)f_unlink(g_temp_path);
    if (f_open(&g_save_file, g_temp_path,
               FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        save_failed(STORAGE_ERROR);
        return STORAGE_RESULT_IO_ERROR;
    }
    if ((f_write(&g_save_file, &g_header, sizeof(g_header), &written) != FR_OK) ||
        (written != sizeof(g_header))) {
        save_failed(STORAGE_ERROR);
        return STORAGE_RESULT_IO_ERROR;
    }
    g_save_offset = 0U;
    g_progress = 0U;
    g_state = STORAGE_SAVING;
    return STORAGE_RESULT_OK;
}

void storage_init(void)
{
    memset(&g_filesystem, 0, sizeof(g_filesystem));
    memset(&g_save_file, 0, sizeof(g_save_file));
    g_pending_capture = NULL;
    g_mounted = mount_card();
    g_state = g_mounted ? STORAGE_READY : STORAGE_NO_CARD;
}

bool storage_mount(void)
{
    if (storage_is_busy()) return false;
    /* Start normally uses the filesystem mounted at boot. Remount only after
       an actual card/transfer failure, not for each recording session. */
    if (g_mounted && disk_status(0U) == 0U) return true;
    g_mounted = mount_card();
    g_state = g_mounted ? ((g_pending_capture != NULL) ? STORAGE_UNSAVED
                                                        : STORAGE_READY)
                        : STORAGE_NO_CARD;
    return g_mounted;
}

void storage_process(void)
{
    if (g_state == STORAGE_SAVING) {
        UINT written;
        size_t length = session_encode_payload_chunk(g_pending_capture,
            g_save_offset, g_io_buffer, sizeof(g_io_buffer));
        if ((length == 0U) ||
            (f_write(&g_save_file, g_io_buffer, (UINT)length, &written) != FR_OK) ||
            (written != length)) {
            save_failed(STORAGE_ERROR);
            return;
        }
        g_save_offset += (uint32_t)length;
        g_progress = (uint8_t)((g_save_offset * 100U) / g_header.payload_size);
        if ((g_save_offset % 4096U) == 0U) {
            if (f_sync(&g_save_file) != FR_OK) {
                save_failed(STORAGE_ERROR);
                return;
            }
        }
        if (g_save_offset >= g_header.payload_size) {
            if ((f_sync(&g_save_file) != FR_OK) ||
                (f_close(&g_save_file) != FR_OK) ||
                (f_rename(g_temp_path, g_final_path) != FR_OK)) {
                save_failed(STORAGE_ERROR);
                return;
            }
            ++g_next_session_id;
            g_pending_capture = NULL;
            g_temp_path[0] = '\0';
            g_progress = 100U;
            g_state = STORAGE_READY;
        }
        return;
    }

    /*
     * HAL_SD_Init performs a long blocking negotiation when no card is
     * present.  Do not run it periodically in the cooperative UI loop.
     * Mount is still attempted at boot, on storage_mount(), and when a save
     * is explicitly requested.
     */
}

storage_state_t storage_get_state(void)
{
    return g_state;
}

storage_state_t storage_status(void) { return storage_get_state(); }

uint8_t storage_get_progress(void)
{
    return g_progress;
}

bool storage_is_busy(void)
{
    return g_state == STORAGE_SAVING;
}

bool storage_has_unsaved_capture(void)
{
    return g_pending_capture != NULL;
}

storage_result_t storage_request_save(const osc_capture_t *capture,
                                      osc_stop_reason_t reason)
{
    if (g_state == STORAGE_SAVING) return STORAGE_RESULT_BUSY;
    if (capture == NULL) return STORAGE_RESULT_INVALID;
    g_pending_capture = capture;
    g_pending_reason = reason;
    return begin_save();
}

storage_result_t storage_retry_save(void)
{
    if (g_pending_capture == NULL) return STORAGE_RESULT_INVALID;
    if (g_state == STORAGE_SAVING) return STORAGE_RESULT_BUSY;
    return begin_save();
}

storage_result_t storage_save(const osc_capture_t *capture,
                              osc_stop_reason_t reason)
{
    return storage_request_save(capture, reason);
}

storage_result_t storage_retry(void) { return storage_retry_save(); }

void storage_discard_unsaved(void)
{
    if (g_state != STORAGE_SAVING) {
        g_pending_capture = NULL;
        g_state = g_mounted ? STORAGE_READY : STORAGE_NO_CARD;
    }
}

static bool find_session(uint32_t session_id, char *path, size_t path_size,
                         osc_session_header_v1_t *header)
{
    DIR directory;
    FILINFO info;
    FRESULT result;
    memset(&info, 0, sizeof(info));
    if (f_opendir(&directory, STORAGE_DIRECTORY) != FR_OK) return false;
    for (;;) {
        uint32_t size;
        result = f_readdir(&directory, &info);
        if ((result != FR_OK) || (info.fname[0] == '\0')) break;
        if ((info.fattrib & AM_DIR) != 0U || !has_osc_extension(file_info_name(&info))) continue;
        (void)snprintf(path, path_size, "%s/%s", STORAGE_DIRECTORY,
                       file_info_name(&info));
        if (read_header_path(path, header, &size) &&
            header->session_id == session_id) {
            (void)f_closedir(&directory);
            return true;
        }
    }
    (void)f_closedir(&directory);
    return false;
}

storage_result_t storage_list(uint32_t cursor, osc_session_info_t *output,
                              uint32_t *next_cursor)
{
    DIR directory;
    FILINFO info;
    FRESULT result;
    uint32_t valid_index = 0U;
    if ((output == NULL) || (next_cursor == NULL)) return STORAGE_RESULT_INVALID;
    if (storage_is_busy() || recording_busy()) {
        return STORAGE_RESULT_BUSY;
    }
    if (!g_mounted) return STORAGE_RESULT_NO_CARD;
    memset(&info, 0, sizeof(info));
    if (f_opendir(&directory, STORAGE_DIRECTORY) != FR_OK) return STORAGE_RESULT_IO_ERROR;
    for (;;) {
        char path[96];
        osc_session_header_v1_t header;
        uint32_t size;
        result = f_readdir(&directory, &info);
        if ((result != FR_OK) || (info.fname[0] == '\0')) break;
        if ((info.fattrib & AM_DIR) != 0U || !has_osc_extension(file_info_name(&info))) continue;
        (void)snprintf(path, sizeof(path), "%s/%s", STORAGE_DIRECTORY,
                       file_info_name(&info));
        if (!read_header_path(path, &header, &size)) continue;
        if (valid_index++ != cursor) continue;
        memset(output, 0, sizeof(*output));
        output->session_id = header.session_id;
        output->timestamp_utc = header.timestamp_utc;
        output->total_length = header.total_length;
        output->sample_rate_hz = header.sample_rate_hz;
        output->sample_count = header.sample_count;
        output->channel_count = header.channel_count;
        output->flags = (uint8_t)header.flags;
        (void)snprintf(output->filename, sizeof(output->filename), "%s",
                       file_info_name(&info));
        *next_cursor = cursor + 1U;
        (void)f_closedir(&directory);
        return STORAGE_RESULT_OK;
    }
    (void)f_closedir(&directory);
    return (result == FR_OK) ? STORAGE_RESULT_NOT_FOUND : STORAGE_RESULT_IO_ERROR;
}

storage_result_t storage_get_info(uint32_t session_id,
                                  osc_session_info_t *output)
{
    char path[96];
    osc_session_header_v1_t header;
    const char *name;
    if (output == NULL) return STORAGE_RESULT_INVALID;
    if (storage_is_busy() || recording_busy()) {
        return STORAGE_RESULT_BUSY;
    }
    if (!g_mounted) return STORAGE_RESULT_NO_CARD;
    if (!find_session(session_id, path, sizeof(path), &header)) {
        return STORAGE_RESULT_NOT_FOUND;
    }
    memset(output, 0, sizeof(*output));
    output->session_id = header.session_id;
    output->timestamp_utc = header.timestamp_utc;
    output->total_length = header.total_length;
    output->sample_rate_hz = header.sample_rate_hz;
    output->sample_count = header.sample_count;
    output->channel_count = header.channel_count;
    output->flags = (uint8_t)header.flags;
    name = strrchr(path, '/');
    if (name != NULL) ++name; else name = path;
    {
        size_t length = strnlen(name, sizeof(output->filename) - 1U);
        memcpy(output->filename, name, length);
        output->filename[length] = '\0';
    }
    return STORAGE_RESULT_OK;
}

storage_result_t storage_read(uint32_t session_id, uint32_t offset,
                              uint8_t *buffer, uint32_t requested,
                              uint32_t *actual)
{
    char path[96];
    osc_session_header_v1_t header;
    FIL file;
    UINT read;
    if ((buffer == NULL) || (actual == NULL)) return STORAGE_RESULT_INVALID;
    if (storage_is_busy() || recording_busy()) return STORAGE_RESULT_BUSY;
    if (!g_mounted) return STORAGE_RESULT_NO_CARD;
    if (!find_session(session_id, path, sizeof(path), &header)) return STORAGE_RESULT_NOT_FOUND;
    if (offset >= header.total_length) {
        *actual = 0U;
        return STORAGE_RESULT_OK;
    }
    if (requested > header.total_length - offset) requested = header.total_length - offset;
    if ((f_open(&file, path, FA_READ) != FR_OK) ||
        (f_lseek(&file, offset) != FR_OK) ||
        (f_read(&file, buffer, requested, &read) != FR_OK)) {
        (void)f_close(&file);
        return STORAGE_RESULT_IO_ERROR;
    }
    (void)f_close(&file);
    *actual = read;
    return STORAGE_RESULT_OK;
}

storage_result_t storage_delete(uint32_t session_id)
{
    char path[96];
    osc_session_header_v1_t header;
    if (storage_is_busy() || recording_busy()) return STORAGE_RESULT_BUSY;
    if (!g_mounted) return STORAGE_RESULT_NO_CARD;
    if (!find_session(session_id, path, sizeof(path), &header)) return STORAGE_RESULT_NOT_FOUND;
    return (f_unlink(path) == FR_OK) ? STORAGE_RESULT_OK : STORAGE_RESULT_IO_ERROR;
}
