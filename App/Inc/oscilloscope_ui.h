#ifndef OSCILLOSCOPE_UI_H
#define OSCILLOSCOPE_UI_H

#include <stdbool.h>
#include <stdint.h>

bool oscilloscope_ui_init(void);
void oscilloscope_ui_process(void);
void oscilloscope_ui_set_storage_status(const char *text);
void oscilloscope_ui_set_save_progress(uint8_t percent);

#endif
