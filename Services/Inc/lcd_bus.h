#ifndef LCD_BUS_H
#define LCD_BUS_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*lcd_bus_dma_callback_t)(bool success, void *context);

void lcd_bus_init(void);
void lcd_bus_delay_ms(uint32_t milliseconds);
void lcd_bus_set_backlight(bool on);
void lcd_bus_write_command(uint8_t command);
void lcd_bus_write_data8(uint8_t data);
void lcd_bus_write_data16(uint16_t data);
uint16_t lcd_bus_read_data16(void);
bool lcd_bus_write_pixels_dma(const uint16_t *pixels, uint32_t count);
bool lcd_bus_write_pixels_dma_async(const uint16_t *pixels, uint32_t count,
                                    lcd_bus_dma_callback_t callback,
                                    void *context);
bool lcd_bus_dma_busy(void);
void lcd_bus_dma_irq_handler(void);

#endif
