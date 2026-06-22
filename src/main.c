#include <stdio.h>
#include "lib_7789.h"
#include "bmp.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/f1/bkp.h>
#include "buttons.h"

static uint32_t freq;                       // Tune frequency in Hz
static uint8_t att;                   // Attenuator state
enum att_values {ATT0, ATT12, ATT24};
static uint8_t modulation;                  // Modulation state
enum modulations {LSB, USB, AM};
static uint8_t bw;
enum bandwidths {b4K8, b2K8};

void encoder_timer_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0 | GPIO1);

    rcc_periph_clock_enable(RCC_TIM2);
    timer_slave_set_mode(TIM2, TIM_SMCR_SMS_EM3);
    timer_set_prescaler(TIM2, 0);
    //timer_set_period(TIM2, 2560);
    timer_enable_counter(TIM2);
}

int16_t encoder_delta(void)
{
    static int16_t encoder_prev;
    int16_t encoder_curr = timer_get_counter(TIM2);
    int16_t encoder_delta = encoder_curr - encoder_prev;
    encoder_prev = encoder_curr;
    return encoder_delta;
}

void lcd_draw_freq_main(uint32_t freq)
{
    uint16_t freq_int = freq/1000;
    uint16_t freq_frac_1 = (freq%1000)/100;
    uint16_t freq_frac_2 = (freq%100)/10;
    char print_buffer[64];
    snprintf(print_buffer, sizeof(print_buffer), " %d.%d%d kHz ", freq_int, freq_frac_1, freq_frac_2);
    lcd_print(161, 50, SCALE_3, ALIGN_CENTER, print_buffer, 0x055F, 0x0025);
}

uint8_t fpga_spi_send(uint32_t freq_word)
{
        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(LCD_CS_PORT, LCD_CS_PIN);
        spi_set_dff_8bit(LCD_SPI);
        gpio_clear(GPIOB, GPIO11);

        uint8_t rx_byte = 0;

        for(int i = 0; i < 4; i++)
        {
            while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
            //if(i == 1) rx_byte = spi_read(LCD_SPI);
            if(i == 0) rx_byte = spi_xfer(LCD_SPI, (freq_word >> (8*(3-i))) & 0xFF);
            else spi_write(LCD_SPI, (freq_word >> (8*(3-i))) & 0xFF);
        }

        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(GPIOB, GPIO11);

        return rx_byte;
}

void s_meter_init_draw(void)
{
    const char* s_meter_nums[9] = {" ", "1", "3", "5", "7", "9", "+12", "+24", "+36"};
    uint16_t nums_x = 20;
    uint16_t nums_x_step = 32;
    uint16_t nums_y = 140;

    lcd_print(nums_x + nums_x_step/2, nums_y, SCALE_1, ALIGN_CENTER, "0", 0x055f, 0x0025);

    for(uint8_t i = 0; i < 9; i++)
    {
        lcd_print(nums_x + nums_x_step*i, nums_y, SCALE_1, ALIGN_CENTER, s_meter_nums[i], 0x055f, 0x0025);
    }
}

void s_meter_print(uint8_t s_value)
{
    char s_value_string[16];

        if(s_value < 10)
        {
            snprintf(s_value_string, 16, "S%d   ", s_value);
        }
        else
        {
            switch(s_value)
            {
                case 10: snprintf(s_value_string, 16, "S+6  "); break;
                case 11: snprintf(s_value_string, 16, "S+12 "); break;
                case 12: snprintf(s_value_string, 16, "S+18 "); break;
                case 13: snprintf(s_value_string, 16, "S+24 "); break;
                case 14: snprintf(s_value_string, 16, "S+30 "); break;
                case 15: snprintf(s_value_string, 16, "S+36 "); break;
                default: snprintf(s_value_string, 16, "error");
            }
        }

        lcd_print(40, 100, SCALE_2, ALIGN_LEFT, s_value_string, 0x055f, 0x0025);
}

void s_meter_bar_draw(uint8_t s_value)
{
    const uint8_t nums_x_step = 32;

    uint16_t s_pixels = (nums_x_step/2)*s_value;

        static uint8_t s_value_prev;
        if(s_value_prev != s_value && s_value <= 15)
        {
        lcd_fill_rect(20+nums_x_step/2, 153, 240, 3, 0x0025);
        
        lcd_fill_rect(20+nums_x_step/2, 153, s_pixels, 3, 0x055f);
        }
        s_value_prev = s_value;
}

void main(void){
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    lcd_init(6);
    lcd_dma_setup();
    encoder_timer_init();
    buttons_setup();

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO11); // FPGA CS pin
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4); // Probe pin
    
    lcd_fill_rect(0,0,320,170,0x0025);  // Main background
    s_meter_init_draw();                // S meter scale numbers

    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_BKP);
    
    freq = (uint32_t)(BKP_DR2 << 16) | (uint32_t)(BKP_DR1);
    if(freq < 100000 || freq > 28000000) freq = 10000000;

    pwr_disable_backup_domain_write_protect();

    uint64_t ph_acc_fs = (uint64_t) 1 << 32;

    bool flush_scale;

    while(1)
    {

        freq = freq + encoder_delta()*5;
        if(freq < 100000) freq = 100000;
        if(freq > 28000000) freq = 28000000;

        lcd_draw_line(160, 5, 160, 9, 0xe8c3);
        lcd_draw_line(160, 19, 160, 23, 0xe8c3);

        if(plus_100k_btn() == BTN_PRS)
        {
            uint32_t remainder = freq%100000;
            if(freq > 27900000) freq = 28000000;
            else
            {
                if(remainder > 0) freq = freq + 100000-remainder;
                else freq = freq + 100000;
            }
            flush_scale = 1;
        }

        if(minus_100k_btn() == BTN_PRS)
        {
            uint32_t remainder = freq%100000;

            if(freq < 200000) freq = 100000;
            else
            {
                if(remainder > 0) freq = freq - remainder;
                else freq = freq - 100000;
            }
            flush_scale = 1;
        }

        if(plus_1M_btn() == BTN_PRS)
        {
            uint32_t remainder = freq%1000000;
            if(freq > 27000000) freq = 28000000;
            else
            {
                if(remainder > 0) freq = freq + 1000000-remainder;
                else freq = freq + 1000000;
            }
            flush_scale = 1;
        }

        if(minus_1M_btn() == BTN_PRS)
        {
            uint32_t remainder = freq%1000000;
            if(freq <= 1000000) freq = 100000;
            else
            {
                if(remainder > 0) freq = freq - remainder;
                else freq = freq - 1000000;
            }
            flush_scale = 1;
        }
        
        lcd_draw_scale(0, 10, 320, 9, freq, flush_scale);
        flush_scale = 0;

        lcd_draw_freq_main(freq);

        double freq_word_float = (double)ph_acc_fs/64.8e6*(double)freq;
        uint32_t freq_word = (uint32_t)freq_word_float;
        
        volatile uint8_t s_value = fpga_spi_send(freq_word);
        
        s_meter_print(s_value);
        s_meter_bar_draw(s_value);

        BKP_DR1 = (uint16_t)(freq & 0xFFFF);
        BKP_DR2 = (uint16_t)(freq >> 16);
        
    }

}