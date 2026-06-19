#include "lib_7789.h"
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <stdio.h>

const uint32_t SPI_RCC;
const uint32_t SPI_ID;
const uint32_t LCD_SPI_PORT_RCC;
const uint32_t LCD_DC_PORT_RCC;
const uint32_t LCD_RESET_PORT_RCC;
const uint32_t LCD_CS_PORT_RCC;

static uint16_t lcd_buffer[8000];
static uint8_t font_ram[91][8];
volatile uint16_t color_static;
volatile bool dma_busy;

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
    spi_enable_tx_dma(LCD_SPI);
    spi_enable(LCD_SPI);
}

void lcd_reset(void)
{
    gpio_clear(LCD_RESET_PORT, LCD_RESET_PIN);
    delay(100000);
    gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
}

void lcd_send_cmd_8(uint8_t cmd)
{
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);
    spi_set_dff_8bit(LCD_SPI);
    gpio_clear(LCD_DC_PORT, LCD_DC_PIN);
    spi_write(LCD_SPI, cmd);
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_set(LCD_CS_PORT, LCD_CS_PIN);
}

void lcd_send_data_8(uint8_t data)
{
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);
    spi_set_dff_8bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    spi_write(LCD_SPI, data);
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_set(LCD_CS_PORT, LCD_CS_PIN);
}

void lcd_send_data_16(uint16_t data)
{
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);
    spi_set_dff_16bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    spi_write(LCD_SPI, data);
    while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
    gpio_set(LCD_CS_PORT, LCD_CS_PIN);
}

void lcd_dma_setup()
{
    rcc_periph_clock_enable(RCC_DMA1);
    dma_disable_channel(DMA1, DMA_CHANNEL3);
    dma_channel_reset(DMA1, DMA_CHANNEL3);
    dma_set_peripheral_address(DMA1, DMA_CHANNEL3, &SPI1_DR);
    dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_16BIT);
    dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_16BIT);
    dma_set_read_from_memory(DMA1, DMA_CHANNEL3);

    nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);
    dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
    dma_busy = false;
}

void dma1_channel3_isr(void)
{
    dma_disable_channel(DMA1, DMA_CHANNEL3);
    if(dma_get_interrupt_flag(DMA1, DMA_CHANNEL3, DMA_TCIF) == true)
    {
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL3, DMA_TCIF);
        dma_busy = false;
    }
}

void lcd_dma_send(uint32_t length)
{
    while(dma_busy == true){}
    spi_set_dff_16bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);
    //dma_disable_channel(DMA1, DMA_CHANNEL3);
    dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
    dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)&lcd_buffer[0]);
    dma_set_number_of_data(DMA1, DMA_CHANNEL3, length);
    dma_busy = true;
    dma_enable_channel(DMA1, DMA_CHANNEL3);
}

void lcd_dma_fill(uint16_t length)
{
    while(dma_busy == true){}
    spi_set_dff_16bit(LCD_SPI);
    gpio_set(LCD_DC_PORT, LCD_DC_PIN);
    gpio_clear(LCD_CS_PORT, LCD_CS_PIN);
    //dma_disable_channel(DMA1, DMA_CHANNEL3);
    dma_disable_memory_increment_mode(DMA1, DMA_CHANNEL3);
    dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)&color_static);
    dma_set_number_of_data(DMA1, DMA_CHANNEL3, length);
    dma_busy = true;
    dma_enable_channel(DMA1, DMA_CHANNEL3);


}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t color)
{
    if(dx > 0 && dy > 0)
    {
        uint16_t x1 = x_crtd(x);
        uint16_t y1 = y_crtd(y);
        uint16_t x2 = x1 + dx-1;
        uint16_t y2 = y1 + dy-1;

        uint16_t packet_length = dx*dy;

        lcd_send_cmd_8(0x2A);
        lcd_send_data_16(x1);
        lcd_send_data_16(x2);
        lcd_send_cmd_8(0x2B);
        lcd_send_data_16(y1);
        lcd_send_data_16(y2);
        lcd_send_cmd_8(0x2C);

        color_static = color;       // Change color ONLY when DMA frees SPI and new coords can be transferred

        lcd_dma_fill(packet_length);
    }
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

void lcd_print(uint16_t x, uint16_t y, uint8_t scale, int alignment, char* string, uint16_t font_color, uint16_t bg_color)
{
    uint8_t font_height = 8;
    uint8_t font_width = 5;

    uint8_t length = strlen(string);
    uint16_t display_width = (font_width+1) * length * scale;
    uint16_t display_height = font_height * scale;

    uint16_t x1 = 0;
    uint16_t y1 = 0;
    uint16_t x2 = 0;
    uint16_t y2 = 0;

    if(alignment == ALIGN_LEFT)
    {
        x1 = x_crtd(x);
        y1 = y_crtd(y);
        x2 = x1 + display_width - 1;
        y2 = y1 + display_height;
    }

    if(alignment == ALIGN_CENTER)
    {
        x1 = x_crtd(x) - display_width/2;
        y1 = y_crtd(y);
        x2 = x1 + display_width - 1;
        y2 = y1 + display_height;
    }

    if(alignment == ALIGN_RIGHT)
    {
        x2 = x_crtd(x);
        y1 = y_crtd(y);
        x1 = x2 - display_width + 2;
        y2 = y1 + display_height;
    }

    uint32_t packet_length = display_width * display_height;
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
                        if((font_ram[char_addr][v_cnt] << shift_cnt) & 0b00010000) lcd_buffer[packet_cnt] = font_color;
                        else lcd_buffer[packet_cnt] = bg_color;
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
    lcd_dma_send(packet_length);

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

void lcd_draw_scale(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint32_t freq)
{

    uint16_t x1 = x_crtd(x);
    uint16_t y1 = y_crtd(y);
    uint16_t x2 = x1 + dx-1;
    uint16_t y2 = y1 + dy-1;

    uint16_t offset_1 = (freq/125)%320;
    uint16_t offset_2 = ((freq/125)+80)%320;
    uint16_t offset_3 = ((freq/125)+160)%320;
    uint16_t offset_4 = ((freq/125)+240)%320;

    uint32_t packet_length = dx * dy;
    uint32_t packet_cnt = 0;

    uint16_t buffer_fine[320];
    uint16_t buffer_coarse[320];

    //for(uint8_t line_cnt = 0; line_cnt < dy; line_cnt++)
    //{
    //    for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
    //    {
    //        if(line_cnt < 6 && (offset_1+pixel_cnt) % 8 == 0) lcd_buffer[packet_cnt] = 0x055f;
    //        else
    //        if((offset_1+pixel_cnt) % 40 == 0) lcd_buffer[packet_cnt] = 0x055f;
    //        else lcd_buffer[packet_cnt] = 0x0025;
    //        packet_cnt++;
    //    }
    //}

    for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
    {
        if((offset_1+pixel_cnt) % 8 == 0) buffer_fine[pixel_cnt] = 0x055f;
        else buffer_fine[pixel_cnt] = 0x0025;

        if((offset_1+pixel_cnt) % 40 == 0) buffer_coarse[pixel_cnt] = 0x055f;
        else buffer_coarse[pixel_cnt] = 0x0025;
    }

    //gpio_set(GPIOA, GPIO4);
/*
    for(uint8_t line_cnt = 0; line_cnt < dy; line_cnt++)
    {
        for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
        {
            if(line_cnt < 5) lcd_buffer[packet_cnt] = buffer_fine[pixel_cnt];
            else lcd_buffer[packet_cnt] = buffer_coarse[pixel_cnt];
            packet_cnt++;
        }
    }
*/
// ############################# QWEN START ##############################
    uint8_t fine_lines = (dy < 5) ? dy : 5; // Защита, если dy < 5
    uint8_t coarse_lines = dy - fine_lines;

    // 1. Копируем точные риски (до 5 строк)
    for(uint8_t line_cnt = 0; line_cnt < fine_lines; line_cnt++)
    {
        for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
        {
            lcd_buffer[packet_cnt++] = buffer_fine[pixel_cnt];
        }
    }

    // 2. Копируем грубые риски (остаток)
    for(uint8_t line_cnt = 0; line_cnt < coarse_lines; line_cnt++)
    {
        for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
        {
            lcd_buffer[packet_cnt++] = buffer_coarse[pixel_cnt];
        }
    }
// ############################## QWEN END ################################

    //gpio_clear(GPIOA, GPIO4);

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    lcd_send_cmd_8(0x2C);

    lcd_dma_send(packet_length);

    uint16_t num_1_pos = 320-(offset_1 % 320);
    uint16_t num_2_pos = 320-(offset_2 % 320);
    uint16_t num_3_pos = 320-(offset_3 % 320);
    uint16_t num_4_pos = 320-(offset_4 % 320);
    
    char num_1[11];
    char num_2[11];
    char num_3[11];
    char num_4[11];

    snprintf(num_1, 11, "   %d   ", (freq-offset_1*125)/1000+20);
    snprintf(num_2, 11, "   %d   ", (freq-offset_2*125)/1000+20);
    snprintf(num_3, 11, "   %d   ", (freq-offset_3*125)/1000+20);
    snprintf(num_4, 11, "   %d   ", (freq-offset_4*125)/1000+20);

    if(num_1_pos > 29 && num_1_pos < 291) lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, num_1, 0x055F, 0x0025);
    else                                  lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, "         ", 0x055F, 0x0025);
    if(num_2_pos > 29 && num_2_pos < 291) lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, num_2, 0x055F, 0x0025);
    else                                  lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, "         ", 0x055F, 0x0025);
    if(num_3_pos > 29 && num_3_pos < 291) lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, num_3, 0x055F, 0x0025);
    else                                  lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, "         ", 0x055F, 0x0025);
    if(num_4_pos > 29 && num_4_pos < 291) lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, num_4, 0x055F, 0x0025);
    else                                  lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, "         ", 0x055F, 0x0025);

}


void lcd_init(uint8_t orientation) // https://github.com/russhughes/st7789_mpy/blob/master/README.md#madctl-constants
{
    memcpy(font_ram, font_44780, sizeof(font_44780));

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