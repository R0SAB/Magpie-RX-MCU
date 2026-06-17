#pragma once
#include <font_44780.h>
#include <config_7789.h>



// ################# High-level fucntions (graphical elements) ####################

void lcd_init(uint8_t orientation); // https://github.com/russhughes/st7789_mpy/blob/master/README.md#madctl-constants
void lcd_draw_bmp(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t* bmp);
void lcd_print(uint16_t x, uint16_t y, uint8_t scale, int alignment, char* string, uint16_t font_color, uint16_t bg_color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t color);
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint8_t thick, uint16_t color);
void lcd_draw_line(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t color);
void lcd_draw_scale(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint32_t freq);
enum print_alignment {ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT};
enum print_scale {SCALE_1 = 1, SCALE_2 = 2, SCALE_3 = 3};

// ################# Low-level fucntions (internal use) ####################

uint16_t x_crtd(uint16_t x);
uint16_t y_crtd(uint16_t y);
void lcd_spi_setup(void);
void lcd_reset(void);
void lcd_send_data_8(uint8_t data);
void lcd_send_data_16(uint16_t data);
void lcd_send_cmd_8(uint8_t cmd);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

