#include "calibration.h"

#include <stddef.h>
#include <string.h>

#include "stm32f4xx_hal.h"
#include "recording.h"

#define CALIBRATION_MAGIC 0x43414C31UL
#define CALIBRATION_VERSION 2U /* PC0-PC2 are now unipolar 47k/10k dividers. */
#define CALIBRATION_FLASH_ADDRESS 0x08060000UL

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    calibration_path_t path[CALIBRATION_PATH_COUNT];
    uint32_t crc32;
} calibration_record_t;

static calibration_record_t g_calibration;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    while (length-- != 0U) {
        uint32_t bit;
        crc ^= *data++;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320UL &
                  (uint32_t)-(int32_t)(crc & 1UL));
        }
    }
    return crc;
}

static uint32_t record_crc(const calibration_record_t *record)
{
    return ~crc32_update(0xFFFFFFFFUL, (const uint8_t *)record,
                         offsetof(calibration_record_t, crc32));
}

static void load_defaults(void)
{
    uint32_t channel;

    memset(&g_calibration, 0, sizeof(g_calibration));
    g_calibration.magic = CALIBRATION_MAGIC;
    g_calibration.version = CALIBRATION_VERSION;
    g_calibration.size = sizeof(g_calibration);
    for (channel = 0U; channel < OSC_MAX_CHANNELS; ++channel) {
        calibration_path_t *direct = &g_calibration.path[channel * 2U];
        calibration_path_t *high = &g_calibration.path[channel * 2U + 1U];
        direct->gain_uv_per_lsb_q16 = 52812995; /* 3.3 V / 4095. */
        direct->offset_uv = 0;
        high->gain_uv_per_lsb_q16 = (int32_t)(18810000LL * 65536LL / 4095LL);
        high->offset_uv = 0; /* 3.3 V ADC full scale times 5.7. */
    }
    g_calibration.crc32 = record_crc(&g_calibration);
}

bool calibration_load(void)
{
    if(recording_busy())return false;
    const calibration_record_t *stored =
        (const calibration_record_t *)CALIBRATION_FLASH_ADDRESS;

    if ((stored->magic == CALIBRATION_MAGIC) &&
        (stored->version == CALIBRATION_VERSION) &&
        (stored->size == sizeof(calibration_record_t)) &&
        (stored->crc32 == record_crc(stored))) {
        memcpy(&g_calibration, stored, sizeof(g_calibration));
        return true;
    } else {
        load_defaults();
        return false;
    }
}

void calibration_init(void) { (void)calibration_load(); }

const calibration_path_t *calibration_get(uint8_t logical_channel,
                                          osc_input_route_t route)
{
    uint32_t index;
    if ((logical_channel >= OSC_MAX_CHANNELS) ||
        ((route != OSC_INPUT_DIRECT) && (route != OSC_INPUT_HIGH_RANGE))) {
        return NULL;
    }
    index = logical_channel * 2U + (uint32_t)route;
    return &g_calibration.path[index];
}

bool calibration_set(uint8_t logical_channel, osc_input_route_t route,
                     const calibration_path_t *value)
{
    if(recording_busy())return false;
    uint32_t index;
    if ((value == NULL) || (logical_channel >= OSC_MAX_CHANNELS) ||
        ((route != OSC_INPUT_DIRECT) && (route != OSC_INPUT_HIGH_RANGE))) {
        return false;
    }
    index = logical_channel * 2U + (uint32_t)route;
    g_calibration.path[index] = *value;
    return true;
}

bool calibration_store(void)
{
    if(recording_busy())return false;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t erase_error = 0U;
    uint32_t address;
    const uint32_t *word;

    ++g_calibration.sequence;
    g_calibration.crc32 = record_crc(&g_calibration);
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_7;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    if (HAL_FLASHEx_Erase(&erase, &erase_error) != HAL_OK) {
        (void)HAL_FLASH_Lock();
        return false;
    }

    address = CALIBRATION_FLASH_ADDRESS;
    word = (const uint32_t *)&g_calibration;
    while (address < CALIBRATION_FLASH_ADDRESS + sizeof(g_calibration)) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *word++) != HAL_OK) {
            (void)HAL_FLASH_Lock();
            return false;
        }
        address += sizeof(uint32_t);
    }
    (void)HAL_FLASH_Lock();
    return memcmp((const void *)CALIBRATION_FLASH_ADDRESS, &g_calibration,
                  sizeof(g_calibration)) == 0;
}

int32_t calibration_apply(const calibration_path_t *calibration,
                          uint16_t raw_sample)
{
    int64_t value;
    if (calibration == NULL) {
        return 0;
    }
    value = (int64_t)raw_sample * calibration->gain_uv_per_lsb_q16;
    return (int32_t)(value / 65536LL) + calibration->offset_uv;
}
