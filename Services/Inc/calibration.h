#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "acquisition.h"

#define CALIBRATION_PATH_COUNT 6U

typedef struct {
    int32_t gain_uv_per_lsb_q16;
    int32_t offset_uv;
} calibration_path_t;

void calibration_init(void);
bool calibration_load(void);
const calibration_path_t *calibration_get(uint8_t logical_channel,
                                          osc_input_route_t route);
bool calibration_set(uint8_t logical_channel, osc_input_route_t route,
                     const calibration_path_t *value);
bool calibration_store(void);
int32_t calibration_apply(const calibration_path_t *calibration,
                          uint16_t raw_sample);

#endif
