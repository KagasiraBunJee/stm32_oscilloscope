#include "touch_bus.h"

#include "stm32f4xx_hal.h"

static void gpio_config_output(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &gpio);
}

static void gpio_config_input_pullup(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);
}

static void touch_clock_delay(void)
{
    /* Conservative clock for the module's long jumper wiring. */
    volatile uint32_t count = 120UL;
    while (count-- != 0UL) {
        __asm volatile ("nop");
    }
}

void touch_bus_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    gpio_config_output(GPIOB, GPIO_PIN_0);        /* T_SCK */
    gpio_config_output(GPIOC, GPIO_PIN_13);       /* T_CS */
    gpio_config_output(GPIOF, GPIO_PIN_11);       /* T_MOSI */
    gpio_config_input_pullup(GPIOB, GPIO_PIN_2);  /* T_MISO */
    gpio_config_input_pullup(GPIOB, GPIO_PIN_1);  /* T_PEN, polled, active low */

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

void touch_bus_select(bool selected)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

uint8_t touch_bus_transfer(uint8_t output)
{
    uint8_t input = 0U;
    uint32_t bit;

    for (bit = 0; bit < 8UL; ++bit) {
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11,
                          (output & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        output <<= 1;
        touch_clock_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        input = (uint8_t)((input << 1) |
                          (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET));
        touch_clock_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    }
    return input;
}

bool touch_bus_pen_is_down(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET;
}
