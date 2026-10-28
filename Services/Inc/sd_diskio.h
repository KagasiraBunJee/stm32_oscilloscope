#ifndef SD_DISKIO_H
#define SD_DISKIO_H

#include "stm32f4xx_hal.h"

SD_HandleTypeDef *sd_diskio_handle(void);
DMA_HandleTypeDef *sd_diskio_rx_dma_handle(void);
DMA_HandleTypeDef *sd_diskio_tx_dma_handle(void);

#endif
