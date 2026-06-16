#include "lib_7789.h"
#include "bmp.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <stdio.h>



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

    lcd_send_cmd_8(0x2A);
    lcd_send_data_16(x1);
    lcd_send_data_16(x2);
    lcd_send_cmd_8(0x2B);
    lcd_send_data_16(y1);
    lcd_send_data_16(y2);

    lcd_send_cmd_8(0x2C);

    for(uint8_t line_cnt = 0; line_cnt < 9; line_cnt++)
    {
        for(uint16_t pixel_cnt = 0; pixel_cnt < 320; pixel_cnt++)
        {
            if(line_cnt < 6 && (offset_1+pixel_cnt) % 8 == 0) lcd_send_data_16(0x055f);
            else
            if((offset_1+pixel_cnt) % 40 == 0) lcd_send_data_16(0x055f);
            else lcd_send_data_16(0x0025);
        }
    }

    uint16_t num_1_pos = 320-(offset_1 % 320);
    uint16_t num_2_pos = 320-(offset_2 % 320);
    uint16_t num_3_pos = 320-(offset_3 % 320);
    uint16_t num_4_pos = 320-(offset_4 % 320);
    
    char num_1[9];
    char num_2[9];
    char num_3[9];
    char num_4[9];

    snprintf(num_1, 9, "  %d  ", (freq-offset_1*125)/1000+20);
    snprintf(num_2, 9, "  %d  ", (freq-offset_2*125)/1000+20);
    snprintf(num_3, 9, "  %d  ", (freq-offset_3*125)/1000+20);
    snprintf(num_4, 9, "  %d  ", (freq-offset_4*125)/1000+20);

    if(num_1_pos > 29 && num_1_pos < 291) lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, num_1, 0x055F, 0x0025);
    else                                  lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, "       ", 0x055F, 0x0025);
    if(num_2_pos > 29 && num_2_pos < 291) lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, num_2, 0x055F, 0x0025);
    else                                  lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, "       ", 0x055F, 0x0025);
    if(num_3_pos > 29 && num_3_pos < 291) lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, num_3, 0x055F, 0x0025);
    else                                  lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, "       ", 0x055F, 0x0025);
    if(num_4_pos > 29 && num_4_pos < 291) lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, num_4, 0x055F, 0x0025);
    else                                  lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, "       ", 0x055F, 0x0025);

}

void lcd_draw_freq_main(uint32_t freq)
{
    uint16_t freq_int = freq/1000;
    uint16_t freq_frac_1 = (freq%1000)/100;
    uint16_t freq_frac_2 = (freq%100)/10;
    char print_buffer[64];
    snprintf(print_buffer, sizeof(print_buffer), " %d.%d%d kHz ", freq_int, freq_frac_1, freq_frac_2);
    lcd_print(161, 50, SCALE_2, ALIGN_CENTER, print_buffer, 0x055F, 0x0025);
}

void main(void){
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(5);

    encoder_timer_init();
 
    lcd_fill_rect(0,0,320,170,0x0025);

    uint16_t encoder_curr = 0;
    uint16_t encoder_prev = 0;
    int16_t encoder_diff = 0;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);

    uint32_t freq = 7077500;
    double freq_float = 1000.555;

    //lcd_draw_freq_main(freq);

    while(1)
    {
        encoder_curr = timer_get_counter(TIM2);
        encoder_diff = encoder_curr - encoder_prev;
        encoder_prev = encoder_curr;
        freq = freq + encoder_diff*5;

        lcd_draw_line(160, 5, 160, 9, 0xe8c3);
        lcd_draw_line(160, 20, 160, 24, 0xe8c3);
        lcd_draw_scale(0, 10, 320, 9, freq);

        gpio_set(GPIOA, GPIO4);
        
        lcd_draw_freq_main(freq);

        gpio_clear(GPIOA, GPIO4);

    }

}