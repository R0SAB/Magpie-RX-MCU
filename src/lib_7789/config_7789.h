#pragma once
#include <stdint.h>

#define LCD_SPI SPI1
#define LCD_SPI_PORT GPIOA
#define LCD_CLK_PIN GPIO5
#define LCD_DATA_PIN GPIO7
 
#define LCD_DC_PORT GPIOB
#define LCD_DC_PIN GPIO1

#define LCD_RESET_PORT GPIOB
#define LCD_RESET_PIN GPIO0

#define LCD_CS_PORT GPIOA
#define LCD_CS_PIN GPIO3

static const uint16_t lcd_offset_x = 0;
static const uint16_t lcd_offset_y = 35;