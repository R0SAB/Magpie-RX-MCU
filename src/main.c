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
#include <time.h>
#include <stdlib.h>

static bool boot_flag = 1;
static uint32_t freq;                       // Tune frequency in Hz
static int32_t correction_ppb;
static uint8_t modulation;
static uint8_t bandwidth;
static uint8_t attenuator;
static uint8_t mode;
enum modulations {MOD_LSB, MOD_USB, MOD_AM};
enum bandwidths {BW_4K8, BW_2K8, BW_0K3};
enum att_values {ATT0, ATT12, ATT24};
enum modes {OPERATION, CORRECTION};

void encoder_timer_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8 | GPIO9);

    rcc_periph_clock_enable(RCC_TIM1);
    timer_slave_set_mode(TIM1, TIM_SMCR_SMS_EM3);
    timer_set_prescaler(TIM1, 0);
    //timer_set_period(TIM1, 2560);
    timer_enable_counter(TIM1);
}

int16_t encoder_delta(void)
{
    static int16_t encoder_prev;
    int16_t encoder_curr = timer_get_counter(TIM1);
    int16_t encoder_delta = encoder_curr - encoder_prev;
    encoder_prev = encoder_curr;
    return encoder_delta;
}

void lcd_print_freq_main(uint32_t freq)
{
    uint16_t freq_int = freq/1000;
    uint16_t freq_frac_1 = (freq%1000)/100;
    uint16_t freq_frac_2 = (freq%100)/10;
    
    static uint8_t mode_prev;

    char print_buffer[64];
    if(mode == OPERATION) snprintf(print_buffer, sizeof(print_buffer), " %d.%d%d kHz ", freq_int, freq_frac_1, freq_frac_2);
    else
    if(mode == CORRECTION) snprintf(print_buffer, sizeof(print_buffer), " Correction: %d.%d ppm ", correction_ppb/1000, (abs(correction_ppb)%1000)/100);

    if(mode_prev == CORRECTION && mode == OPERATION) snprintf(print_buffer, sizeof(print_buffer), "                      ");

    lcd_print(161, 40, SCALE_2, ALIGN_CENTER, print_buffer, 0x055F, 0x0025);

    mode_prev = mode;
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

    lcd_print(200, 80, SCALE_1, ALIGN_LEFT, "MOD: LSB USB  AM", 0x055f, 0x0025);
    lcd_print(200, 95, SCALE_1, ALIGN_LEFT, " BW: 4.8 2.8 0.3", 0x055f, 0x0025);
    lcd_print(200, 110, SCALE_1, ALIGN_LEFT, "ATT:  0  -12 -24", 0x055f, 0x0025);
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

        lcd_print(30, 110, SCALE_2, ALIGN_LEFT, s_value_string, 0x055f, 0x0025);
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

void modes_routine(uint16_t color, uint16_t bg_color)
{
    
    if(mode == OPERATION)
    {
        if(mod_btn() == BTN_RLS && !boot_flag)
        {
            switch(modulation)
            {
                case MOD_LSB: modulation = MOD_USB; break;
                case MOD_USB: modulation =  MOD_AM; break;
                case MOD_AM:  modulation = MOD_LSB; break;
                default: modulation = MOD_LSB;
            }

            lcd_fill_rect(230, 89, 18, 2, (modulation == MOD_LSB) ? color:bg_color);  // MOD LSB
            lcd_fill_rect(254, 89, 18, 2, (modulation == MOD_USB) ? color:bg_color);  // MOD USB
            lcd_fill_rect(278, 89, 18, 2, (modulation == MOD_AM)  ? color:bg_color);  // MOD AM

            BKP_DR3 = (uint16_t)modulation & 0x00FF;
        }

        if(bw_btn() == BTN_RLS && !boot_flag)
        {
            switch(bandwidth)
            {
                case BW_4K8: bandwidth = BW_2K8; break;
                case BW_2K8: bandwidth = BW_0K3; break;
                case BW_0K3: bandwidth = BW_4K8; break;
                default: bandwidth = BW_4K8;
            }

            lcd_fill_rect(230, 104, 18, 2, (bandwidth == BW_4K8) ? color:bg_color);   // BW 4K8
            lcd_fill_rect(254, 104, 18, 2, (bandwidth == BW_2K8) ? color:bg_color);   // BW 2k8
            lcd_fill_rect(278, 104, 18, 2, (bandwidth == BW_0K3) ? color:bg_color);   // BW 0K3

            BKP_DR4 = (uint16_t)bandwidth & 0x00FF;
        }

        if(att_btn() == BTN_RLS && !boot_flag)
        {
            switch(attenuator)
            {
                case ATT0:  attenuator = ATT12; break;
                case ATT12: attenuator = ATT24; break;
                case ATT24: attenuator =  ATT0; break;
                default: attenuator = ATT0;
            }

            lcd_fill_rect(230, 119, 18, 2, (attenuator ==  ATT0) ? color:bg_color);   // ATT 0
            lcd_fill_rect(254, 119, 18, 2, (attenuator == ATT12) ? color:bg_color);   // ATT -12
            lcd_fill_rect(278, 119, 18, 2, (attenuator == ATT24) ? color:bg_color);   // ATT -24

            BKP_DR5 = (uint16_t)attenuator & 0x00FF;
        }
    }

    if(boot_flag == 1)
    {
        modulation = BKP_DR3;
        bandwidth = BKP_DR4;
        attenuator = BKP_DR5;

        lcd_fill_rect(230, 89, 18, 2, (modulation == MOD_LSB) ? color:bg_color);  // MOD LSB
        lcd_fill_rect(254, 89, 18, 2, (modulation == MOD_USB) ? color:bg_color);  // MOD USB
        lcd_fill_rect(278, 89, 18, 2, (modulation == MOD_AM)  ? color:bg_color);  // MOD AM

        lcd_fill_rect(230, 104, 18, 2, (bandwidth == BW_4K8) ? color:bg_color);   // BW 4K8
        lcd_fill_rect(254, 104, 18, 2, (bandwidth == BW_2K8) ? color:bg_color);   // BW 2k8
        lcd_fill_rect(278, 104, 18, 2, (bandwidth == BW_0K3) ? color:bg_color);   // BW 0K3

        lcd_fill_rect(230, 119, 18, 2, (attenuator ==  ATT0) ? color:bg_color);   // ATT 0
        lcd_fill_rect(254, 119, 18, 2, (attenuator == ATT12) ? color:bg_color);   // ATT -12
        lcd_fill_rect(278, 119, 18, 2, (attenuator == ATT24) ? color:bg_color);   // ATT -24
    }

    static uint16_t btn_delay;

    if(mode == OPERATION && mod_btn() == BTN_HLD)
    {
        if(btn_delay < 200) btn_delay++;
        if(btn_delay == 200) mode = CORRECTION;
    }
    else btn_delay = 0;

    if(mode == CORRECTION && mod_btn() == BTN_RLS) mode = OPERATION;
   
}

bool freq_buttons_polling(void)
{
    if(plus_100k_btn() == BTN_RLS && !boot_flag)
    {
        uint32_t remainder = freq%100000;
        if(freq > 27900000) freq = 28000000;
        else
        {
            if(remainder > 0) freq = freq + 100000-remainder;
            else freq = freq + 100000;
        }
        return 1;
    }

    if(minus_100k_btn() == BTN_RLS && !boot_flag)
    {
        uint32_t remainder = freq%100000;

        if(freq < 200000) freq = 100000;
        else
        {
            if(remainder > 0) freq = freq - remainder;
            else freq = freq - 100000;
        }
        return 1;
    }

    if(plus_1M_btn() == BTN_RLS && !boot_flag)
    {
        uint32_t remainder = freq%1000000;
        if(freq > 27000000) freq = 28000000;
        else
        {
            if(remainder > 0) freq = freq + 1000000-remainder;
            else freq = freq + 1000000;
        }
        return 1;
    }

    if(minus_1M_btn() == BTN_RLS && !boot_flag)
    {
        uint32_t remainder = freq%1000000;
        if(freq <= 1000000) freq = 100000;
        else
        {
            if(remainder > 0) freq = freq - remainder;
            else freq = freq - 1000000;
        }
        return 1;
    }

    return 0;
    
}

void main(void){
    
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    lcd_init(6);
    lcd_dma_setup();
    encoder_timer_init();
    buttons_setup();
    
    volatile uint32_t stup_delay = 100000;
    while(stup_delay > 0) stup_delay--;

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO11); // FPGA CS pin
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4); // Probe pin
    
    lcd_fill_rect(0,0,320,170,0x0025);  // Main background
    s_meter_init_draw();                // S meter scale numbers

    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_BKP);
    
    mode = OPERATION;

    freq = (uint32_t)(BKP_DR2 << 16) | (uint32_t)(BKP_DR1);
    if(freq < 100000 || freq > 28000000) freq = 10000000;

    modulation = BKP_DR3;
    bandwidth = BKP_DR4;
    attenuator = BKP_DR5;

    correction_ppb = (uint32_t)(BKP_DR7 << 16) | (uint32_t)(BKP_DR6);
    if(correction_ppb < -100000 || correction_ppb > 100000) correction_ppb = 10000000;

    pwr_disable_backup_domain_write_protect();

    uint64_t ph_acc_fs = (uint64_t) 1 << 32;

    bool flush_scale;

    modes_routine(0x3d40, 0x0025);
    freq_buttons_polling();


    boot_flag = 0;

    while(1)
    {

        if(mode == OPERATION) freq = freq + encoder_delta()*5;
        else
        if(mode == CORRECTION)
        {
            correction_ppb += encoder_delta();
            BKP_DR6 = (uint16_t)(correction_ppb & 0xFFFF);
            BKP_DR7 = (uint16_t)(correction_ppb >> 16);
        }

        if(freq < 100000) freq = 100000;
        if(freq > 28000000) freq = 28000000;

        lcd_draw_line(160, 0, 160, 4, 0xe8c3);
        lcd_draw_line(160, 14, 160, 18, 0xe8c3);

        flush_scale = freq_buttons_polling();
        lcd_draw_scale(0, 5, 320, 9, freq, flush_scale);
        flush_scale = 0;

        lcd_print_freq_main(freq);

        double freq_word_float = (double)ph_acc_fs/64.8e6*(double)freq*(double)(1+correction_ppb*1e-9);
        uint32_t freq_word = (uint32_t)freq_word_float;
        
        volatile uint8_t s_value = fpga_spi_send(freq_word);
        
        s_meter_print(s_value);
        s_meter_bar_draw(s_value);

        modes_routine(0x3d40, 0x0025);

        BKP_DR1 = (uint16_t)(freq & 0xFFFF);
        BKP_DR2 = (uint16_t)(freq >> 16);
        
    }

}