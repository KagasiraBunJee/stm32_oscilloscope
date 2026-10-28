#ifndef STM32F4XX_IT_H
#define STM32F4XX_IT_H

void SysTick_Handler(void);
void DMA2_Stream0_IRQHandler(void);
void DMA2_Stream1_IRQHandler(void);
void DMA2_Stream3_IRQHandler(void);
void DMA2_Stream6_IRQHandler(void);
void SDIO_IRQHandler(void);
void OTG_FS_IRQHandler(void);

#endif
