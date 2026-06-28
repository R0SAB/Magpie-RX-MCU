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
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>


static bool boot_flag = 1;
static uint32_t freq;                       // Tune frequency in Hz
static int32_t correction_ppb;
static uint8_t modulation;
static uint8_t bandwidth;
static uint8_t attenuator;
static uint8_t mode;
static uint8_t time_field;
enum modulations {MOD_LSB = 0, MOD_USB = 1, MOD_AM = 2};
enum bandwidths {BW_4K8 = 0, BW_2K8 = 1, BW_0K3 = 2};
enum att_values {ATT0, ATT12, ATT24};
enum modes {OPERATION, CORRECTION, TIME_SET};

enum time_modes {SEC, MIN, HOUR, DAY, MONTH, YEAR};
char time_string[32];
char date_string[32];
time_t timestamp = 0;
int timestamp_int = 0;
int timestamp_int_prev = 0;
struct tm time_struct =
{
    .tm_year = 2019 - 1900,
    .tm_mon  = 10 - 1,       
    .tm_mday = 26,
    .tm_hour = 19,
    .tm_min  = 23,
    .tm_sec  = 0,
    .tm_isdst = 0
};

void encoder_timer_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8 | GPIO9);

    rcc_periph_clock_enable(RCC_TIM1);
    timer_slave_set_mode(TIM1, TIM_SMCR_SMS_EM3);
    timer_set_prescaler(TIM1, 0);
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

void voltmeter_setup(void)
{
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO1);
    rcc_periph_clock_enable(RCC_ADC1);
    adc_power_off(ADC1);
    adc_disable_dma(ADC1);
    adc_disable_scan_mode(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_55DOT5CYC);
    adc_set_regular_sequence(ADC1, 1, (uint8_t[]){ADC_CHANNEL1});
    adc_set_single_conversion_mode(ADC1);
    adc_set_right_aligned(ADC1);
    adc_power_on(ADC1);
    adc_calibrate(ADC1);
}

void voltmeter_routine()
{
    float R_upper = 5.1e3f;
    float R_lower = 2.7e3f;
    float V_ref = 3e0f;

    char voltage_string[16];

    static uint8_t frame_counter;
    if(frame_counter < 19) frame_counter++;
    else frame_counter = 0;

    if(frame_counter == 0) adc_start_conversion_direct(ADC1);

    if(frame_counter == 1)
    {
        uint16_t adc_value = adc_read_regular(ADC1);

        float volt_float = (float)adc_value / (float)4096 * V_ref * (1.0f + R_upper / R_lower);
        uint16_t V_mv = (uint16_t)(volt_float * 1000);
        uint8_t volt_int = V_mv / 1000;
        uint8_t volt_frac_1 = (V_mv % 1000) / 100;
        uint8_t volt_frac_2 = (V_mv % 100) / 10;
        
        snprintf(voltage_string, sizeof(voltage_string), "Vbat:%d.%d%dV", volt_int, volt_frac_1, volt_frac_2);
        lcd_print(35, 98, SCALE_1, ALIGN_LEFT, voltage_string, 0x055f, 0x0025);
    }
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
    if(mode == CORRECTION)
    {
        uint8_t ppm_int = abs(correction_ppb)/1000;
        uint8_t ppm_frac = (abs(correction_ppb)%1000)/100;
        if(correction_ppb/100 < 0) snprintf(print_buffer, sizeof(print_buffer), " Correction: -%d.%d ppm ", ppm_int, ppm_frac);
        else                       snprintf(print_buffer, sizeof(print_buffer), " Correction: %d.%d ppm " , ppm_int, ppm_frac);
    }
    else
    if(mode == TIME_SET) snprintf(print_buffer, sizeof(print_buffer), "    Time set...    ");

    if(mode_prev == CORRECTION && mode == OPERATION) snprintf(print_buffer, sizeof(print_buffer), "                      ");

    lcd_print(161, 40, SCALE_2, ALIGN_CENTER, print_buffer, 0x055F, 0x0025);

    mode_prev = mode;
}

void draw_visor(void)
{
    static uint8_t modulation_prev;
    static uint8_t bandwidth_prev;

    uint16_t color = /*0x3d40*/0x2bc0;
    uint16_t bg_color = 0x0025;

    if(modulation != modulation_prev || bandwidth != bandwidth_prev)
    {
        lcd_fill_rect(160-38, 1, 77, 3, bg_color);  // clear whole +- 4/8 kHz range

        if(modulation == MOD_AM)
        {
            
            if(bandwidth == BW_4K8) lcd_fill_rect(160-38, 1, 77, 3, color);
            if(bandwidth == BW_2K8) lcd_fill_rect(160-22, 1, 45, 3, color);
        }

        if(modulation == MOD_LSB)
        {
            
            if(bandwidth == BW_4K8) lcd_fill_rect(160-38, 1, 38, 3, color);
            if(bandwidth == BW_2K8) lcd_fill_rect(160-22, 1, 22, 3, color);
            if(bandwidth == BW_0K3) lcd_fill_rect(160-9, 1, 3, 3, color);
        }

        if(modulation == MOD_USB)
        {
            
            if(bandwidth == BW_4K8) lcd_fill_rect(160+1, 1, 38, 3, color);
            if(bandwidth == BW_2K8) lcd_fill_rect(160+1, 1, 22, 3, color);
            if(bandwidth == BW_0K3) lcd_fill_rect(160+7, 1, 3, 3, color);
        }

        lcd_draw_line(160, 0, 160, 4, 0xe8c3);
        lcd_draw_line(160, 14, 160, 18, 0xe8c3);
    }

    modulation_prev = modulation;
    bandwidth_prev = bandwidth;
}

uint8_t fpga_spi_send(uint32_t freq_word)
{
        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(LCD_CS_PORT, LCD_CS_PIN);
        spi_set_dff_8bit(LCD_SPI);
        gpio_clear(GPIOB, GPIO11);

        uint8_t fpga_agc_byte = 0;
        uint8_t fpga_ctrl_byte = ((0b11 & bandwidth) << 2) | (0b11 & modulation);

        fpga_agc_byte = spi_xfer(LCD_SPI, fpga_ctrl_byte);
        for(int i = 0; i < 4; i++)
        {
            while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
            spi_write(LCD_SPI, (freq_word >> (8*(3-i))) & 0xFF);
        }

        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(GPIOB, GPIO11);

        return fpga_agc_byte;
}

void static_elements_draw(void)
{
    const char* s_meter_nums[9] = {" ", "1", "3", "5", "7", "9", "+12", "+24", "+36"};
    uint16_t nums_x = 20;
    uint16_t nums_x_step = 32;
    uint16_t nums_y = 150;

    lcd_print(nums_x + nums_x_step/2, nums_y, SCALE_1, ALIGN_CENTER, "0", 0x055f, 0x0025);

    for(uint8_t i = 0; i < 9; i++)
    {
        lcd_print(nums_x + nums_x_step*i, nums_y, SCALE_1, ALIGN_CENTER, s_meter_nums[i], 0x055f, 0x0025);
    }

    lcd_print(200, 80, SCALE_1, ALIGN_LEFT, "MOD: LSB USB ", 0x055f, 0x0025);
    lcd_print(200, 95, SCALE_1, ALIGN_LEFT, " BW: 4.8 2.8 ", 0x055f, 0x0025);
    lcd_print(200, 110, SCALE_1, ALIGN_LEFT, "ATT:  0  -12 -24", 0x055f, 0x0025);
}

void rtc_and_bkp_init(void)
{
    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_BKP);
    pwr_disable_backup_domain_write_protect();
    
    if(!rcc_rtc_clock_enabled_flag())
    {
        rcc_set_rtc_clock_source(RCC_LSE);
        rcc_enable_rtc_clock();
        rtc_set_prescale_val(32767);
        rtc_set_counter_val(1572121800); // Sat Oct 26 2019 20:30:00
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

        lcd_print(20, 120, SCALE_2, ALIGN_LEFT, s_value_string, 0x055f, 0x0025);
}

void s_meter_bar_draw(uint8_t s_value)
{
    const uint8_t nums_x_step = 32;

    uint16_t s_pixels = (nums_x_step/2)*s_value;

        static uint8_t s_value_prev;
        if(s_value_prev != s_value && s_value <= 15)
        {
        lcd_fill_rect(20+nums_x_step/2, 143, 240, 3, 0x0025);
        
        lcd_fill_rect(20+nums_x_step/2, 143, s_pixels, 3, 0x055f);
        }
        s_value_prev = s_value;
}

void lcd_print_time(void)
{
    strftime(time_string, sizeof(time_string), "%H:%M:%S UTC ", &time_struct);
    strftime(date_string, sizeof(date_string), "%d-%m-%Y" , &time_struct);
    lcd_print(20, 70, SCALE_1, ALIGN_LEFT, time_string, 0x055F, 0x0025);
    lcd_print(20, 80, SCALE_1, ALIGN_LEFT, date_string, 0x055F, 0x0025);
}

void modes_routine(uint16_t color, uint16_t bg_color)
{
    
    if(mode == OPERATION)                           // Nomal operation of MOD, BW, ATT buttons
    {
        if(mod_btn() == BTN_RLS && !boot_flag)       
        {
            switch(modulation)
            {
                case MOD_LSB: modulation = MOD_USB; break;
                case MOD_USB: modulation = (bandwidth == BW_0K3)? MOD_LSB : MOD_AM; break;
                case MOD_AM:  modulation = MOD_LSB; break;
                default: modulation = MOD_LSB;
            }

            //lcd_fill_rect(230, 89, 18, 2, (modulation == MOD_LSB) ? color:bg_color);  // MOD LSB
            //lcd_fill_rect(254, 89, 18, 2, (modulation == MOD_USB) ? color:bg_color);  // MOD USB
            //lcd_fill_rect(278, 89, 18, 2, (modulation == MOD_AM)  ? color:bg_color);  // MOD AM

            BKP_DR3 = (uint16_t)modulation & 0x00FF;
        }

        if(bw_btn() == BTN_RLS && !boot_flag)
        {
            switch(bandwidth)
            {
                case BW_4K8: bandwidth = BW_2K8; break;
                case BW_2K8: bandwidth = (modulation == MOD_AM) ? BW_4K8 : BW_0K3; break;
                case BW_0K3: bandwidth = BW_4K8; break;
                default: bandwidth = BW_4K8;
            }

            //lcd_fill_rect(230, 104, 18, 2, (bandwidth == BW_4K8) ? color:bg_color);   // BW 4K8
            //lcd_fill_rect(254, 104, 18, 2, (bandwidth == BW_2K8) ? color:bg_color);   // BW 2k8
            //lcd_fill_rect(278, 104, 18, 2, (bandwidth == BW_0K3) ? color:bg_color);   // BW 0K3

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

            //lcd_fill_rect(230, 119, 18, 2, (attenuator ==  ATT0) ? color:bg_color);   // ATT 0
            //lcd_fill_rect(254, 119, 18, 2, (attenuator == ATT12) ? color:bg_color);   // ATT -12
            //lcd_fill_rect(278, 119, 18, 2, (attenuator == ATT24) ? color:bg_color);   // ATT -24

            BKP_DR5 = (uint16_t)attenuator & 0x00FF;
        }
    }


    if(boot_flag == 1)                              // Initial draw of mode indicators
    {
        modulation = BKP_DR3;
        bandwidth = BKP_DR4;
        attenuator = BKP_DR5;
    }

    static uint16_t btn_delay_mod;

    if(mode == OPERATION && mod_btn() == BTN_HLD)   // Alternate function of MOD - CORRECTION of ref frequency (press and hold)
    {
        if(btn_delay_mod < 200) btn_delay_mod++;
        if(btn_delay_mod == 200) mode = CORRECTION;
    }
    else btn_delay_mod = 0;

    if(mode == CORRECTION && mod_btn() == BTN_RLS) mode = OPERATION;    // Exit CORRECTION - upon release of MOD
   
    static uint16_t btn_delay_bw;

    if(mode == OPERATION && bw_btn() == BTN_HLD)    // Alternate function of BW - long press and release to enter TIME_SET
    {
        if(btn_delay_bw < 200) btn_delay_bw++;
        if(btn_delay_bw == 200)
        {
            mode = TIME_SET;
            time_field = HOUR;
        }
    }
    else btn_delay_bw = 0;

    if(mode == TIME_SET && bw_btn() == BTN_PRS)     // Exit TIME_SET - single press of BW
    {
        mode = OPERATION;

        timestamp = mktime(&time_struct);
        timestamp_int = (int32_t)timestamp;
        rtc_set_counter_val(timestamp_int);


        lcd_fill_rect(20, 78, 12, 1, bg_color);
        lcd_fill_rect(38, 78, 12, 1, bg_color);
        lcd_fill_rect(56, 78, 12, 1, bg_color);
        lcd_fill_rect(20, 88, 12, 1, bg_color);
        lcd_fill_rect(38, 88, 12, 1, bg_color);
        lcd_fill_rect(56, 88, 24, 1, bg_color);
    }
    


    if(mode == TIME_SET)                            // Time adjustment routine
    {
        if(att_btn() == BTN_RLS)
        {
            switch(time_field)
            {
                case HOUR: time_field = MIN; break;
                case MIN: time_field = SEC; break;
                case SEC: time_field = DAY; break;
                case DAY: time_field = MONTH; break;
                case MONTH: time_field = YEAR; break;
                case YEAR: time_field = HOUR; break;
                default: time_field = SEC;
            }
        }

        //time_struct.tm_sec += encoder_delta();
        mktime(&time_struct);

        lcd_fill_rect(20, 78, 12, 1, (time_field ==  HOUR)? color : bg_color);
        lcd_fill_rect(38, 78, 12, 1, (time_field ==   MIN)? color : bg_color);
        lcd_fill_rect(56, 78, 12, 1, (time_field ==   SEC)? color : bg_color);
        lcd_fill_rect(20, 88, 12, 1, (time_field ==   DAY)? color : bg_color);
        lcd_fill_rect(38, 88, 12, 1, (time_field == MONTH)? color : bg_color);
        lcd_fill_rect(56, 88, 24, 1, (time_field ==  YEAR)? color : bg_color);

        if(minus_100k_btn() == BTN_RLS)
        {
            if(time_field ==   SEC) time_struct.tm_sec++;
            if(time_field ==   MIN) time_struct.tm_min++;
            if(time_field ==  HOUR) time_struct.tm_hour++;
            if(time_field ==   DAY) time_struct.tm_mday++;
            if(time_field == MONTH) time_struct.tm_mon++;
            if(time_field ==  YEAR) time_struct.tm_year++;
        }

        if(minus_1M_btn() == BTN_RLS)
        {
            if(time_field ==   SEC) time_struct.tm_sec--;
            if(time_field ==   MIN) time_struct.tm_min--;
            if(time_field ==  HOUR) time_struct.tm_hour--;
            if(time_field ==   DAY) time_struct.tm_mday--;
            if(time_field == MONTH) time_struct.tm_mon--;
            if(time_field ==  YEAR) time_struct.tm_year--;
        }

        lcd_print_time();

    }

    lcd_print(278, 80, SCALE_1, ALIGN_LEFT, " AM",(bandwidth != BW_0K3)? 0x055F:0xB211, 0x0025);
    lcd_print(278, 95, SCALE_1, ALIGN_LEFT, "0.3",(modulation != MOD_AM)? 0x055F:0xB211, 0x0025);

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

bool freq_buttons_polling(void)
{
    if(mode == OPERATION)
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
    }
    return 0;
    
}

void main(void){
    
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    lcd_init(6);
    lcd_dma_setup();
    encoder_timer_init();
    buttons_setup();
    rtc_and_bkp_init();

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO11); // FPGA CS pin
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4); // Probe pin
    
    lcd_fill_rect(0,0,320,170,0x0025);  // Main background
    static_elements_draw();                // S meter scale numbers
    
    mode = OPERATION;

    freq = (uint32_t)(BKP_DR2 << 16) | (uint32_t)(BKP_DR1);
    if(freq < 100000 || freq > 28000000) freq = 10000000;

    modulation = BKP_DR3;
    bandwidth = BKP_DR4;
    attenuator = BKP_DR5;

    correction_ppb = (uint32_t)(BKP_DR7 << 16) | (uint32_t)(BKP_DR6);
    if(correction_ppb < -100000 || correction_ppb > 100000) correction_ppb = 10000000;

    uint64_t ph_acc_fs = (uint64_t) 1 << 32;

    bool flush_scale;

    modes_routine(0x3d40, 0x0025);
    freq_buttons_polling();

    voltmeter_setup();

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

        flush_scale = freq_buttons_polling();
        lcd_draw_scale(0, 5, 320, 9, freq, flush_scale);
        flush_scale = 0;

        draw_visor();

        lcd_print_freq_main(freq);

        double freq_word_float = (double)ph_acc_fs/65e6*(double)freq*(double)(1+correction_ppb*1e-9);
        uint32_t freq_word = (uint32_t)freq_word_float;
        
        volatile uint8_t s_value = fpga_spi_send(freq_word);
        
        s_meter_print(s_value);
        s_meter_bar_draw(s_value);

        modes_routine(0x3d40, 0x0025);

        timestamp_int = rtc_get_counter_val();
        if(timestamp_int != timestamp_int_prev && mode == OPERATION)
        {
            timestamp_int_prev = timestamp_int;
            timestamp = (time_t)timestamp_int;
            localtime_r(&timestamp, &time_struct);

            lcd_print_time();
        }

        //lcd_print(35, 98, SCALE_1, ALIGN_LEFT, "Vbat:8.15V", 0x055f, 0x0025);

        voltmeter_routine();

        BKP_DR1 = (uint16_t)(freq & 0xFFFF);
        BKP_DR2 = (uint16_t)(freq >> 16);
        
        
    }

}