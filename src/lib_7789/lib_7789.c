#include "lib_7789.h"
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

const uint32_t SPI_RCC;
const uint32_t SPI_ID;
const uint32_t LCD_SPI_PORT_RCC;
const uint32_t LCD_DC_PORT_RCC;
const uint32_t LCD_RESET_PORT_RCC;
const uint32_t LCD_CS_PORT_RCC;


#if LCD_SPI == SPI1
    const uint32_t SPI_RCC = RCC_SPI1;
#elif LCD_SPI == SPI2
    const uint32_t SPI_ID = RCC_SPI1;
#endif

#if LCD_SPI_PORT == GPIOA
    const uint32_t LCD_SPI_PORT_RCC = RCC_GPIOA;
#elif LCD_DC_PORT == GPIOB
    const uint32_t LCD_SPI_PORT_RCC = RCC_GPIOB;
#elif LCD_DC_PORT == GPIOC
    const uint32_t LCD_SPI_PORT_RCC = RCC_GPIOC;
#endif

#if LCD_DC_PORT == GPIOA
    const uint32_t LCD_DC_PORT_RCC = RCC_GPIOA;
#elif LCD_DC_PORT == GPIOB
    const uint32_t LCD_DC_PORT_RCC = RCC_GPIOB;
#elif LCD_DC_PORT == GPIOC
    const uint32_t LCD_DC_PORT_RCC = RCC_GPIOC;
#endif

#if LCD_RESET_PORT == GPIOA
    const uint32_t LCD_RESET_PORT_RCC = RCC_GPIOA;
#elif LCD_RESET_PORT == GPIOB
    const uint32_t LCD_RESET_PORT_RCC = RCC_GPIOB;
#elif LCD_RESET_PORT == GPIOC
    const uint32_t LCD_RESET_PORT_RCC = RCC_GPIOC;
#endif

#if LCD_CS_PORT == GPIOA
    const uint32_t LCD_CS_PORT_RCC = RCC_GPIOA;
#elif LCD_CS_PORT == GPIOB
    const uint32_t LCD_CS_PORT_RCC = RCC_GPIOB;
#elif LCD_CS_PORT == GPIOC
    const uint32_t LCD_CS_PORT_RCC = RCC_GPIOC;
#endif


uint16_t x_crtd(uint16_t x)
{
    return x + lcd_offset_x;
}

uint16_t y_crtd(uint16_t y)
{
    return y + lcd_offset_y;
}

void delay(uint32_t delay_cycles)
{
    volatile uint32_t counter;
    counter = 0;
    while(counter < delay_cycles) counter++;
}

void lcd_spi_setup(void)
{
    rcc_periph_clock_enable(SPI_RCC);
    rcc_periph_clock_enable(LCD_SPI_PORT_RCC);
    rcc_periph_clock_enable(LCD_DC_PORT_RCC);
    rcc_periph_clock_enable(LCD_RESET_PORT_RCC);
    rcc_periph_clock_enable(LCD_CS_PORT_RCC);

    gpio_set_mode(LCD_SPI_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, LCD_CLK_PIN | LCD_DATA_PIN);
    gpio_set_mode(LCD_DC_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LCD_DC_PIN);
    gpio_set_mode(LCD_RESET_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LCD_RESET_PIN);
    gpio_set_mode(LCD_CS_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LCD_CS_PIN);

    spi_init_master
    (
        LCD_SPI,
        SPI_CR1_BAUDRATE_FPCLK_DIV_2,
        SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
        SPI_CR1_CPHA_CLK_TRANSITION_2,
        SPI_CR1_DFF_8BIT,
        SPI_CR1_MSBFIRST
    );
    spi_enable(LCD_SPI);
}

void lcd_reset(void)
{
    gpio_clear(LCD_RESET_PORT, LCD_RESET_PIN);
    delay(100000);
    gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
}

void lcd_send_data_8(uint8_t data)
{
    //while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
    spi_set_dff_8bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    spi_send(LCD_SPI, data);
}

void lcd_send_data_16(uint16_t data)
{
    while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
    spi_set_dff_16bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    spi_send(LCD_SPI, data);
}

void lcd_send_packet_16(uint16_t* data, uint32_t length)
{
    //while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
    spi_set_dff_16bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);

    for(uint32_t a = 0; a < length; a++) spi_send(LCD_SPI, data[a]);
}

void lcd_send_cmd_8(uint8_t cmd)
{
    //while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
    spi_set_dff_8bit(LCD_SPI);
    gpio_clear(LCD_DC_PORT, LCD_DC_PIN);
    spi_send(LCD_SPI, cmd);
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t color)
{
    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);
    uint16_t x2 = x1 + dx-1;
    uint16_t y2 = y1 + dy-1;

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    uint32_t pixel_cnt = 0;

    lcd_send_cmd_8(0x2C);

    while(pixel_cnt < (dx*dy))
    {
        pixel_cnt++;
        lcd_send_data_16(color);
    };
}

void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint8_t thick, uint16_t color)
{
    uint16_t x1 = x_crtd(x);
    uint16_t x2 = x1+dx-1;
    uint16_t y1 = y_crtd(y);
    uint16_t y2 = y2+thick-1;
    
    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    uint32_t pixel_cnt = 0;

    lcd_send_cmd_8(0x2C);
    while(pixel_cnt < (dx*thick))
    {
        lcd_send_data_16(color);
        pixel_cnt++;
    }

    x1 = x_crtd(x);
    x2 = x1+thick-1;
    y1 = y_crtd(y);
    y2 = y1+dy-1;

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    pixel_cnt = 0;

    lcd_send_cmd_8(0x2C);
    while(pixel_cnt < (dy*thick))
    {
        lcd_send_data_16(color);
        pixel_cnt++;
    }

    x1 = x_crtd(x);
    x2 = x1+dx-1;
    y2 = y_crtd(y)+dy-1;
    y1 = y2-thick+1;

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    pixel_cnt = 0;

    lcd_send_cmd_8(0x2C);
    while(pixel_cnt < (dx*thick))
    {
        lcd_send_data_16(color);
        pixel_cnt++;
    }

    x2 = x_crtd(x)+dx-1;
    x1 = x2-thick+1;
    y1 = y_crtd(y);
    y2 = y2+thick-1;

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    pixel_cnt = 0;

    lcd_send_cmd_8(0x2C);
    while(pixel_cnt < (dy*thick))
    {
        lcd_send_data_16(color);
        pixel_cnt++;
    }

}



void lcd_print(uint16_t x, uint16_t y, uint8_t scale, char* string, uint16_t font_color, uint16_t bg_color)
{
    uint8_t font_height = 8;
    uint8_t font_width = 5;

    uint8_t length = strlen(string);

    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);
    uint16_t x2 = x1 + ((font_width + 1) * length) * scale - 1;
    uint16_t y2 = y1 + font_height * scale;

    uint32_t packet_length = (font_height * scale) * ((font_width+1) * scale) * length;
    uint16_t packet[10000];
    uint32_t packet_cnt = 0;

    for(uint8_t v_cnt = 0; v_cnt < font_height; v_cnt++)    
    {
        for(uint8_t scale_v_cnt = 0; scale_v_cnt < scale; scale_v_cnt++)
        {
            for(uint8_t char_cnt = 0; char_cnt < length; char_cnt++)
            {
                uint8_t char_addr = string[char_cnt] - 0x20;

                for(uint8_t shift_cnt = 0; shift_cnt < font_width + 1; shift_cnt++)
                {
                    for(uint8_t scale_h_cnt = 0; scale_h_cnt < scale; scale_h_cnt++)
                    {
                        if((font_44780[char_addr][v_cnt] << shift_cnt) & 0b00010000) packet[packet_cnt] = font_color;
                        else packet[packet_cnt] = bg_color;
                        packet_cnt++;
                    }
                }
            }
        }
    }

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    lcd_send_cmd_8(0x2C);
    lcd_send_packet_16(packet, packet_length);

}

void lcd_draw_bmp(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t* bmp)
{
    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);
    uint16_t x2 = x1 + dx-1;
    uint16_t y2 = y1 + dy-1;

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    lcd_send_cmd_8(0x2C);

    for(uint32_t pixel_cnt = 0; pixel_cnt < (dx*dy); pixel_cnt++)
    {
        lcd_send_data_16(bmp[pixel_cnt]);
    }
}




void lcd_init(uint8_t orientation) // https://github.com/russhughes/st7789_mpy/blob/master/README.md#madctl-constants
{
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);

    lcd_spi_setup();

    uint8_t orientation_byte = 0;

    switch (orientation)
    {
    case 0: orientation_byte = 0x00; break;
    case 1: orientation_byte = 0x80; break;
    case 2: orientation_byte = 0x40; break;
    case 3: orientation_byte = 0xC0; break;
    case 4: orientation_byte = 0x20; break;
    case 5: orientation_byte = 0xA0; break;
    case 6: orientation_byte = 0x60; break;
    case 7: orientation_byte = 0xE0; break;
    default: orientation_byte = 0x00; break;
    }

    lcd_reset();
    delay(10000);
    lcd_send_cmd_8(0x01);   //SWRESET
    delay(10000);
    lcd_send_cmd_8(0x11);   //SLPOUT
    delay(10000);
       
    lcd_send_cmd_8(0x3A);   // COLMOD, 
    lcd_send_data_8(0x05);      // RGB565(16bit) 0x05,
       
    lcd_send_cmd_8(0x36);   //MADCTL
    lcd_send_data_8(orientation_byte);
       
    lcd_send_cmd_8(0x21);   //INVON

    lcd_send_cmd_8(0x13);   //NORON
       
    lcd_send_cmd_8(0x29);   //DISPON
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);
    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_cmd_8(0x2C);
    lcd_send_data_16(color);
}


void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    int16_t dx = (x2 > x1) ? (int16_t)(x2 - x1) : (int16_t)(x1 - x2);
    int16_t dy = (y2 > y1) ? (int16_t)(y2 - y1) : (int16_t)(y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1) {
        lcd_draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}