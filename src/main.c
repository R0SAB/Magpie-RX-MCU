#include <stdint.h>
#include <libopencm3/stm32/memorymap.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <string.h>

#include "font_44780.h"


#define LCD_SPI SPI1
#define LCD_SPI_PORT GPIOA
#define LCD_CLK_PIN GPIO5
#define LCD_DATA_PIN GPIO7
 
#define LCD_DC_PORT GPIOB
#define LCD_DC_PIN GPIO1

#define LCD_RESET_PORT GPIOB
#define LCD_RESET_PIN GPIO0

#define LCD_CS_PORT GPIOB
#define LCD_CS_PIN GPIO10

static const uint16_t lcd_offset_x = 0;
static const uint16_t lcd_offset_y = 35;

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

void spi_setup(void)
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
    while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
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

void lcd_send_cmd_8(uint8_t cmd)
{
    while(SPI_SR(LCD_SPI) & SPI_SR_BSY);
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

void lcd_print(uint16_t x, uint16_t y, char* string, uint16_t font_color, uint16_t bg_color)
{
    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);

    uint8_t font_height = 8;
    uint8_t font_width = 5;

    uint8_t length = strlen(string);

    for(uint8_t char_cnt = 0; char_cnt < length; char_cnt++)
    {
        x1 = x1+(font_width+1);
        uint16_t x2 = x1+(font_width);

        lcd_send_cmd_8(0x2A);
        lcd_send_data_16(x1);
        lcd_send_data_16(x2);
        lcd_send_cmd_8(0x2B);
        lcd_send_data_16(y1);
        lcd_send_data_16(y1+font_height);

        uint16_t pixel_cnt = 0;

        lcd_send_cmd_8(0x2C);
        while(pixel_cnt < (font_height*(font_width+1)))
        {
            pixel_cnt++;
            lcd_send_data_16(font_color);
        }
    }
}





void lcd_init(uint8_t orientation) // https://github.com/russhughes/st7789_mpy/blob/master/README.md#madctl-constants
{
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);

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


void main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    delay(10000);

    spi_setup();

    delay(10000);

    lcd_init(5);

    delay(10000);
    
    lcd_fill_rect(0,0,320,170,0x055F);

    lcd_print(20, 20, "ABCDEFG", 0x5F00, 0x0000);
}