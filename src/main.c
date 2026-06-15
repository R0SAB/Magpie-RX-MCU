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

    uint16_t offset = (freq/125)%320;

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
            if(line_cnt < 6 && (offset+pixel_cnt) % 8 == 0) lcd_send_data_16(0x055f);
            else
            if((offset+pixel_cnt) % 40 == 0) lcd_send_data_16(0x055f);
            else lcd_send_data_16(0x0025);
        }
    }

    uint16_t num_1_pos = 320-(offset % 320);
    uint16_t num_2_pos = 320-((offset + 80) % 320);
    uint16_t num_3_pos = 320-((offset + 160) % 320);
    uint16_t num_4_pos = 320-((offset + 240) % 320);

    if(num_1_pos > 29 && num_1_pos < 291) lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, " 12345 ", 0x055F, 0x0025);
    else  lcd_print(num_1_pos, 25, SCALE_1, ALIGN_CENTER, "        ", 0x055F, 0x0025);
    if(num_2_pos > 29 && num_2_pos < 291) lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, " 12345 ", 0x055F, 0x0025);
    else  lcd_print(num_2_pos, 25, SCALE_1, ALIGN_CENTER, "        ", 0x055F, 0x0025);
    if(num_3_pos > 29 && num_3_pos < 291) lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, " 12345 ", 0x055F, 0x0025);
    else  lcd_print(num_3_pos, 25, SCALE_1, ALIGN_CENTER, "        ", 0x055F, 0x0025);
    if(num_4_pos > 29 && num_4_pos < 291) lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, " 12345 ", 0x055F, 0x0025);
    else  lcd_print(num_4_pos, 25, SCALE_1, ALIGN_CENTER, "        ", 0x055F, 0x0025);

}

void main(void){
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(5);

    encoder_timer_init();
 
    lcd_fill_rect(0,0,320,170,0x0025);

    uint32_t offset = 0;
    uint16_t encoder_curr = 0;
    uint16_t encoder_prev = 0;
    int16_t encoder_diff = 0;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);

    char freq_print[64];
    uint8_t disp_counter = 0;

    uint32_t freq = 7077500;
    double freq_float = 0;

    while(1)
    {
        //if(offset < 319) offset+=1;
        //else offset = 0;
        encoder_curr = timer_get_counter(TIM2);
        encoder_diff = encoder_curr - encoder_prev;
        encoder_prev = encoder_curr;
        freq = freq + encoder_diff*5;
        offset = freq/125;
        
        lcd_draw_line(160, 5, 160, 9, 0xe8c3);
        lcd_draw_line(160, 20, 160, 24, 0xe8c3);
        lcd_draw_scale(0, 10, 320, 9, freq);

        freq_float = (float)(freq/1000.0);

        gpio_set(GPIOA, GPIO4);
        //snprintf(freq_print, sizeof(freq_print), " %0.2f kHz ", freq_float);
        lcd_print(161, 50, SCALE_3, ALIGN_CENTER, freq_print, 0x055F, 0x0025);
        gpio_clear(GPIOA, GPIO4);

    }

}