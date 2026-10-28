# Drivers

This directory contains the official STM32CubeF4 drivers:

- `CMSIS/` for ARM CMSIS-Core and the STM32F4 device package.
- `STM32F4xx_HAL_Driver/` for ST HAL/LL sources and headers.

Application code uses the service and board abstractions over
`stm32f4xx_hal.h`; no private replacement peripheral definitions are used.
