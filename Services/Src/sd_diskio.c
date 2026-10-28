#include "sd_diskio.h"

#include <string.h>

#include "board.h"
#include "diskio.h"
#include "rtc_service.h"
#include "recording.h"

#define SD_BLOCK_SIZE 512U
#define SD_OPERATION_TIMEOUT_MS 5000U

static SD_HandleTypeDef g_sd;
static DMA_HandleTypeDef g_sd_rx_dma;
static DMA_HandleTypeDef g_sd_tx_dma;
static volatile bool g_rx_done;
static volatile bool g_tx_done;
static volatile bool g_sd_error;
static DSTATUS g_status = STA_NOINIT | STA_NODISK;
static uint8_t g_bounce[SD_BLOCK_SIZE] __attribute__((aligned(4)));

static bool wait_transfer(volatile bool *completed)
{
    uint32_t start = board_millis();
    while (!*completed && !g_sd_error) {
        /* Let normal DMA transfers finish before starting an expensive render.
           A slow card still receives cooperative UI service after 10 ms. */
        if ((uint32_t)(board_millis() - start) >= 10U) recording_io_yield();
        if ((uint32_t)(board_millis() - start) >= SD_OPERATION_TIMEOUT_MS) {
            goto abort;
        }
    }
    if (g_sd_error) goto abort;
    while (HAL_SD_GetCardState(&g_sd) != HAL_SD_CARD_TRANSFER) {
        if ((uint32_t)(board_millis() - start) >= 10U) recording_io_yield();
        if ((uint32_t)(board_millis() - start) >= SD_OPERATION_TIMEOUT_MS) {
            goto abort;
        }
    }
    return true;
abort:
    (void)HAL_SD_Abort(&g_sd);
    (void)HAL_DMA_Abort(&g_sd_rx_dma);
    (void)HAL_DMA_Abort(&g_sd_tx_dma);
    g_status=STA_NOINIT;
    return false;
}

static bool read_block(uint8_t *buffer, uint32_t sector, uint32_t count)
{
    g_rx_done = false;
    g_sd_error = false;
    if (HAL_SD_ReadBlocks_DMA(&g_sd, buffer, sector, count) != HAL_OK) {
        g_status = STA_NOINIT;
        return false;
    }
    return wait_transfer(&g_rx_done);
}

static bool write_block(const uint8_t *buffer, uint32_t sector, uint32_t count)
{
    g_tx_done = false;
    g_sd_error = false;
    if (HAL_SD_WriteBlocks_DMA(&g_sd, (uint8_t *)buffer, sector, count) != HAL_OK) {
        g_status = STA_NOINIT;
        return false;
    }
    return wait_transfer(&g_tx_done);
}

static bool card_init(void)
{
    /* A new software handle does not reset SDIO's FIFO/data counter or its
       interrupt mask. Quiesce the previous instance before negotiating SCR. */
    if (g_sd.Instance != NULL) {
        HAL_NVIC_DisableIRQ(SDIO_IRQn);
        HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
        HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
        g_sd.Instance->MASK = 0U;
        g_sd.Instance->DCTRL = 0U;
        (void)HAL_DMA_Abort(&g_sd_rx_dma);
        (void)HAL_DMA_Abort(&g_sd_tx_dma);
        (void)HAL_SD_DeInit(&g_sd);
    }
    __HAL_RCC_SDIO_CLK_ENABLE();
    __HAL_RCC_SDIO_FORCE_RESET();
    __HAL_RCC_SDIO_RELEASE_RESET();
    HAL_NVIC_ClearPendingIRQ(SDIO_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream6_IRQn);
    g_rx_done = false;
    g_tx_done = false;
    g_sd_error = false;
    memset(&g_sd, 0, sizeof(g_sd));
    g_sd.Instance = SDIO;
    g_sd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    g_sd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    g_sd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    g_sd.Init.BusWide = SDIO_BUS_WIDE_1B;
    g_sd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    g_sd.Init.ClockDiv = SDIO_TRANSFER_CLK_DIV;
    if (HAL_SD_Init(&g_sd) != HAL_OK) {
        (void)HAL_SD_DeInit(&g_sd);
        return false;
    }
    if (HAL_SD_ConfigWideBusOperation(&g_sd, SDIO_BUS_WIDE_4B) != HAL_OK) {
        (void)HAL_SD_DeInit(&g_sd);
        return false;
    }
    g_status = 0U;
    return true;
}

DSTATUS disk_initialize(BYTE drive)
{
    if (drive != 0U) return STA_NOINIT;
    if (!card_init()) {
        g_status = STA_NOINIT | STA_NODISK;
    }
    return g_status;
}

DSTATUS disk_status(BYTE drive)
{
    if (drive != 0U) return STA_NOINIT;
    if ((g_status & STA_NOINIT) == 0U &&
        HAL_SD_GetCardState(&g_sd) == HAL_SD_CARD_ERROR) {
        g_status = STA_NOINIT | STA_NODISK;
    }
    return g_status;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, DWORD sector, UINT count)
{
    UINT block;
    if ((drive != 0U) || (buffer == NULL) || (count == 0U) ||
        ((g_status & STA_NOINIT) != 0U)) return RES_NOTRDY;
    uintptr_t address=(uintptr_t)buffer;
    if((address&3U)==0 && address>=0x20000000UL &&
       count<=128U && address+(uintptr_t)count*512U<=0x20020000UL)
        return read_block(buffer,sector,count)?RES_OK:RES_ERROR;
    for (block = 0U; block < count; ++block) {
        if (!read_block(g_bounce, sector + block, 1U)) {
            g_status = STA_NOINIT;
            return RES_ERROR;
        }
        memcpy(buffer + block * SD_BLOCK_SIZE, g_bounce, SD_BLOCK_SIZE);
    }
    return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count)
{
    UINT block;
    if ((drive != 0U) || (buffer == NULL) || (count == 0U) ||
        ((g_status & STA_NOINIT) != 0U)) return RES_NOTRDY;
    uintptr_t address=(uintptr_t)buffer;
    if((address&3U)==0 && address>=0x20000000UL &&
       count<=128U && address+(uintptr_t)count*512U<=0x20020000UL)
        return write_block(buffer,sector,count)?RES_OK:RES_ERROR;
    for (block = 0U; block < count; ++block) {
        memcpy(g_bounce, buffer + block * SD_BLOCK_SIZE, SD_BLOCK_SIZE);
        if (!write_block(g_bounce, sector + block, 1U)) {
            g_status = STA_NOINIT;
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
    HAL_SD_CardInfoTypeDef info;
    if ((drive != 0U) || ((g_status & STA_NOINIT) != 0U)) return RES_NOTRDY;
    switch (command) {
    case CTRL_SYNC:
        return (HAL_SD_GetCardState(&g_sd) == HAL_SD_CARD_TRANSFER)
            ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
        if ((buffer == NULL) || (HAL_SD_GetCardInfo(&g_sd, &info) != HAL_OK)) {
            return RES_ERROR;
        }
        *(DWORD *)buffer = info.LogBlockNbr;
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buffer == NULL) return RES_PARERR;
        *(WORD *)buffer = SD_BLOCK_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buffer == NULL) return RES_PARERR;
        *(DWORD *)buffer = 1U;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    return rtc_service_fattime();
}

void HAL_SD_MspInit(SD_HandleTypeDef *handle)
{
    GPIO_InitTypeDef gpio = {0};
    if (handle->Instance != SDIO) return;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_SDIO_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_SDIO;
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &gpio);

    g_sd_rx_dma.Instance = DMA2_Stream3;
    g_sd_rx_dma.Init.Channel = DMA_CHANNEL_4;
    g_sd_rx_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_sd_rx_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    g_sd_rx_dma.Init.MemInc = DMA_MINC_ENABLE;
    g_sd_rx_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    g_sd_rx_dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    g_sd_rx_dma.Init.Mode = DMA_PFCTRL;
    g_sd_rx_dma.Init.Priority = DMA_PRIORITY_HIGH;
    g_sd_rx_dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    g_sd_rx_dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    g_sd_rx_dma.Init.MemBurst = DMA_MBURST_INC4;
    g_sd_rx_dma.Init.PeriphBurst = DMA_PBURST_INC4;
    (void)HAL_DMA_Init(&g_sd_rx_dma);

    g_sd_tx_dma.Instance = DMA2_Stream6;
    g_sd_tx_dma.Init = g_sd_rx_dma.Init;
    g_sd_tx_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    (void)HAL_DMA_Init(&g_sd_tx_dma);
    __HAL_LINKDMA(handle, hdmarx, g_sd_rx_dma);
    __HAL_LINKDMA(handle, hdmatx, g_sd_tx_dma);

    HAL_NVIC_SetPriority(SDIO_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 4U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *handle)
{
    if (handle->Instance != SDIO) return;
    HAL_NVIC_DisableIRQ(SDIO_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
    (void)HAL_DMA_DeInit(&g_sd_rx_dma);
    (void)HAL_DMA_DeInit(&g_sd_tx_dma);
    __HAL_RCC_SDIO_CLK_DISABLE();
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *handle)
{
    if (handle == &g_sd) g_tx_done = true;
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *handle)
{
    if (handle == &g_sd) g_rx_done = true;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *handle)
{
    if (handle == &g_sd) g_sd_error = true;
}

SD_HandleTypeDef *sd_diskio_handle(void)
{
    return &g_sd;
}

DMA_HandleTypeDef *sd_diskio_rx_dma_handle(void)
{
    return &g_sd_rx_dma;
}

DMA_HandleTypeDef *sd_diskio_tx_dma_handle(void)
{
    return &g_sd_tx_dma;
}
