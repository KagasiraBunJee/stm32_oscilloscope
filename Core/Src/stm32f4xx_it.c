#include "stm32f4xx_it.h"

#include "stm32f4xx_hal.h"
#include "acquisition.h"
#include "lcd_bus.h"
#include "sd_diskio.h"
#include "usb_device.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(acquisition_dma_handle());
}

void ADC_IRQHandler(void)
{
    acquisition_adc_irq();
}

void DMA2_Stream1_IRQHandler(void)
{
    lcd_bus_dma_irq_handler();
}

void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(sd_diskio_rx_dma_handle());
}

void DMA2_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(sd_diskio_tx_dma_handle());
}

void SDIO_IRQHandler(void)
{
    HAL_SD_IRQHandler(sd_diskio_handle());
}

void OTG_FS_IRQHandler(void)
{
    usb_device_irq_handler();
}
