#include "lcd_bus.h"

#include <stddef.h>

#include "board.h"
#include "stm32f4xx_hal.h"

/* BLACK_F407ZE J6 / MRB3205 16-bit FSMC interface. */
#define LCD_COMMAND_ADDRESS 0x6C00007EUL
#define LCD_DATA_ADDRESS    0x6C000080UL
#define LCD_BL_PIN          15U
#define FSMC_AF             12U

#define LCD_COMMAND (*(volatile uint16_t *)LCD_COMMAND_ADDRESS)
#define LCD_DATA    (*(volatile uint16_t *)LCD_DATA_ADDRESS)
#define FSMC_BWTR4  (*(volatile uint32_t *)0xA000011CUL)

#define DMA_STREAM_ENABLE       (1UL << 0)
#define DMA_STREAM_TCIE         (1UL << 4)
#define DMA_STREAM_TEIE         (1UL << 2)
#define DMA_STREAM_DMEIE        (1UL << 1)
#define DMA_STREAM_PINC         (1UL << 9)
#define DMA_STREAM_PSIZE_16BIT  (1UL << 11)
#define DMA_STREAM_MSIZE_16BIT  (1UL << 13)
#define DMA_STREAM_PRIORITY_HIGH (2UL << 16)
#define DMA_STREAM_MEMORY_TO_MEMORY (2UL << 6)
#define DMA_FIFO_ENABLE         (1UL << 2)
#define DMA_FIFO_FULL           3UL

#define DMA_STREAM1_FEIF  (1UL << 6)
#define DMA_STREAM1_DMEIF (1UL << 8)
#define DMA_STREAM1_TEIF  (1UL << 9)
#define DMA_STREAM1_HTIF  (1UL << 10)
#define DMA_STREAM1_TCIF  (1UL << 11)
#define DMA_STREAM1_ALL_FLAGS \
    (DMA_STREAM1_FEIF | DMA_STREAM1_DMEIF | DMA_STREAM1_TEIF | \
     DMA_STREAM1_HTIF | DMA_STREAM1_TCIF)
#define DMA_STREAM1_ERROR_FLAGS \
    (DMA_STREAM1_FEIF | DMA_STREAM1_DMEIF | DMA_STREAM1_TEIF)

volatile uint32_t lcd_dma_completed_transfers;
volatile uint32_t lcd_dma_last_status;
static volatile bool g_dma_async_busy;
static lcd_bus_dma_callback_t g_dma_async_callback;
static void *g_dma_async_context;

static void gpio_config_af(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = FSMC_AF;
    HAL_GPIO_Init(port, &gpio);
}

static void gpio_config_output(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &gpio);
}

static void dma_abort(void)
{
    uint32_t timeout = BOARD_CORE_CLOCK_HZ / 1000UL;

    DMA2_Stream1->CR &= ~DMA_STREAM_ENABLE;
    while (((DMA2_Stream1->CR & DMA_STREAM_ENABLE) != 0UL) &&
           (timeout-- != 0UL)) {
    }
    DMA2->LIFCR = DMA_STREAM1_ALL_FLAGS;
}

static bool dma_write_chunk(const uint16_t *pixels, uint16_t count)
{
    const uint32_t start = DWT->CYCCNT;
    uint32_t status;

    dma_abort();
    DMA2_Stream1->PAR = (uint32_t)(uintptr_t)pixels;
    DMA2_Stream1->M0AR = LCD_DATA_ADDRESS;
    DMA2_Stream1->NDTR = count;
    DMA2_Stream1->FCR = DMA_FIFO_ENABLE | DMA_FIFO_FULL;
    DMA2_Stream1->CR = DMA_STREAM_MEMORY_TO_MEMORY |
                       DMA_STREAM_PINC |
                       DMA_STREAM_PSIZE_16BIT |
                       DMA_STREAM_MSIZE_16BIT |
                       DMA_STREAM_PRIORITY_HIGH;

    __DSB();
    DMA2_Stream1->CR |= DMA_STREAM_ENABLE;

    for (;;) {
        status = DMA2->LISR;
        if ((status & DMA_STREAM1_ERROR_FLAGS) != 0UL) {
            lcd_dma_last_status = status;
            dma_abort();
            return false;
        }
        if ((status & DMA_STREAM1_TCIF) != 0UL) {
            break;
        }
        if ((uint32_t)(DWT->CYCCNT - start) >
            (BOARD_CORE_CLOCK_HZ / 2UL)) {
            lcd_dma_last_status = 0x80000000UL | status;
            dma_abort();
            return false;
        }
    }

    lcd_dma_last_status = status;
    ++lcd_dma_completed_transfers;
    DMA2->LIFCR = DMA_STREAM1_ALL_FLAGS;
    return true;
}

void lcd_bus_init(void)
{
    static const uint16_t port_d_pins[] = {
        GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_8,
        GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_14, GPIO_PIN_15
    };
    uint32_t index;
    uint32_t pin;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, (uint16_t)(1U << LCD_BL_PIN), GPIO_PIN_RESET);
    gpio_config_output(GPIOB, (uint16_t)(1U << LCD_BL_PIN));

    for (index = 0U; index < (sizeof(port_d_pins) / sizeof(port_d_pins[0]));
         ++index) {
        gpio_config_af(GPIOD, port_d_pins[index]);
    }
    for (pin = 7U; pin <= 15U; ++pin) {
        gpio_config_af(GPIOE, (uint16_t)(1U << pin));
    }
    gpio_config_af(GPIOF, GPIO_PIN_12);
    gpio_config_af(GPIOG, GPIO_PIN_12);

    FSMC_Bank1->BTCR[6] &= ~1UL;
    FSMC_Bank1->BTCR[6] = (1UL << 4) |  /* 16-bit bus */
                              (1UL << 12) | /* writes enabled */
                              (1UL << 14);  /* extended write timing */
    FSMC_Bank1->BTCR[7] = 15UL | (60UL << 8); /* read timing */
    FSMC_BWTR4 = 9UL | (8UL << 8);            /* safe write timing */
    FSMC_Bank1->BTCR[6] |= 1UL;

    dma_abort();
    lcd_dma_completed_transfers = 0UL;
    lcd_dma_last_status = 0UL;
    g_dma_async_busy = false;
    g_dma_async_callback = NULL;
    g_dma_async_context = NULL;
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    __DSB();
}

void lcd_bus_delay_ms(uint32_t milliseconds)
{
    board_delay_ms(milliseconds);
}

void lcd_bus_set_backlight(bool on)
{
    HAL_GPIO_WritePin(GPIOB, (uint16_t)(1U << LCD_BL_PIN),
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void lcd_bus_write_command(uint8_t command)
{
    LCD_COMMAND = command;
}

void lcd_bus_write_data8(uint8_t data)
{
    LCD_DATA = data;
}

void lcd_bus_write_data16(uint16_t data)
{
    LCD_DATA = data;
}

uint16_t lcd_bus_read_data16(void)
{
    return LCD_DATA;
}

bool lcd_bus_write_pixels_dma(const uint16_t *pixels, uint32_t count)
{
    if ((pixels == NULL) || (count == 0UL)) {
        return false;
    }

    while (count != 0UL) {
        const uint16_t chunk =
            (count > 65535UL) ? 65535U : (uint16_t)count;

        if (!dma_write_chunk(pixels, chunk)) {
            return false;
        }
        pixels += chunk;
        count -= chunk;
    }
    return true;
}

bool lcd_bus_write_pixels_dma_async(const uint16_t *pixels, uint32_t count,
                                    lcd_bus_dma_callback_t callback,
                                    void *context)
{
    if ((pixels == NULL) || (count == 0UL) || (count > 65535UL) ||
        g_dma_async_busy) {
        return false;
    }

    dma_abort();
    g_dma_async_busy = true;
    g_dma_async_callback = callback;
    g_dma_async_context = context;
    DMA2_Stream1->PAR = (uint32_t)(uintptr_t)pixels;
    DMA2_Stream1->M0AR = LCD_DATA_ADDRESS;
    DMA2_Stream1->NDTR = count;
    DMA2_Stream1->FCR = DMA_FIFO_ENABLE | DMA_FIFO_FULL;
    DMA2_Stream1->CR = DMA_STREAM_MEMORY_TO_MEMORY |
                       DMA_STREAM_PINC |
                       DMA_STREAM_PSIZE_16BIT |
                       DMA_STREAM_MSIZE_16BIT |
                       DMA_STREAM_PRIORITY_HIGH |
                       DMA_STREAM_TCIE | DMA_STREAM_TEIE |
                       DMA_STREAM_DMEIE;
    __DSB();
    DMA2_Stream1->CR |= DMA_STREAM_ENABLE;
    return true;
}

bool lcd_bus_dma_busy(void)
{
    return g_dma_async_busy;
}

void lcd_bus_dma_irq_handler(void)
{
    uint32_t status = DMA2->LISR;
    bool success;
    lcd_bus_dma_callback_t callback;
    void *context;

    if (!g_dma_async_busy ||
        ((status & (DMA_STREAM1_TCIF | DMA_STREAM1_ERROR_FLAGS)) == 0U)) {
        DMA2->LIFCR = DMA_STREAM1_ALL_FLAGS;
        return;
    }

    success = (status & DMA_STREAM1_ERROR_FLAGS) == 0U;
    lcd_dma_last_status = status;
    if (success) {
        ++lcd_dma_completed_transfers;
    }
    DMA2_Stream1->CR &= ~DMA_STREAM_ENABLE;
    DMA2->LIFCR = DMA_STREAM1_ALL_FLAGS;
    callback = g_dma_async_callback;
    context = g_dma_async_context;
    g_dma_async_callback = NULL;
    g_dma_async_context = NULL;
    g_dma_async_busy = false;
    if (callback != NULL) {
        callback(success, context);
    }
}
