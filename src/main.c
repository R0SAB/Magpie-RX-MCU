#include "lib_7789.h"
#include "bmp.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

/*
void lcd_draw_scale(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t* bmp)
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
*/

uint32_t freq = 0;

void lcd_draw_scale(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t offset)
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
}

void encoder_timer_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0 | GPIO1);

    rcc_periph_clock_enable(RCC_TIM2);
    timer_slave_set_mode(TIM2, TIM_SMCR_SMS_EM3);
    timer_set_prescaler(TIM2, 1);
    //timer_set_period(TIM2, 2560);
    timer_enable_counter(TIM2);
}

void main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(5);

    encoder_timer_init();
 
    lcd_fill_rect(0,0,320,170,0x0025);

    uint32_t offset = 0;
    uint16_t encoder_curr = 0;
    uint16_t encoder_prev = 0;
    int16_t encoder_diff = 0;

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
        lcd_draw_scale(0, 10, 320, 9, offset);

        //lcd_print(290-offset, 50, 1, "Hyzeth", 0x055f, 0x0025);
    }
    //lcd_draw_bmp(0, 10, 320, 2, scale_bitmap);
    //
    //lcd_print(43, 11, 2, "Hyzeth", 0x055F, 0x0025);
    //
    //lcd_draw_bmp(160, 10, 150, 150, bitmap);
    //
    //lcd_draw_rect(155, 5, 160,160 ,2,0x055F);
    //
    //lcd_print(5, 35, 1,  "Race: Rito", 0x055F, 0x0025);
    //lcd_print(5, 45, 1,  "Gender: male", 0x055F, 0x0025);
    //lcd_print(5, 55, 1,  "Home: Rito Village", 0x055F, 0x0025);
    //lcd_print(5, 65, 1,  "Personality: gentle,", 0x055F, 0x0025);
    //lcd_print(5, 75, 1,  "      careful, calm, sad", 0x055F, 0x0025);
    //lcd_print(5, 85, 1,  "Occupation: ancient tech", 0x055F, 0x0025);
    //lcd_print(5, 95, 1,  "      (Sheikah and Zonai)", 0x055F, 0x0025);
    //lcd_print(5, 105, 1, "Hobby: music (Hammond", 0x055F, 0x0025);
    //lcd_print(5, 115, 1, "               organ)", 0x055F, 0x0025);
    //lcd_print(5, 125, 1, "Special traits: mute", 0x055F, 0x0025);
    //lcd_print(5, 135, 1, "Other: sheltered some", 0x055F, 0x0025);
    //lcd_print(5, 145, 1, "      hylian, with which", 0x055F, 0x0025);
    //lcd_print(5, 155, 1, "      is in relationship", 0x055F, 0x0025);
    //
    //lcd_draw_line(4, 43, 5+6*5-3, 43, 0x055F);
    //lcd_draw_line(4, 53, 5+6*7-3, 53, 0x055F);
    //lcd_draw_line(4, 63, 5+6*5-3, 63, 0x055F);
    //lcd_draw_line(4, 73, 5+6*12-3, 73, 0x055F);
    //lcd_draw_line(4, 93, 5+6*11-3, 93, 0x055F);
    //lcd_draw_line(4, 113, 5+6*6-3, 113, 0x055F);
    //lcd_draw_line(4, 133, 5+6*15-3, 133, 0x055F);
    //lcd_draw_line(4, 143, 5+6*6-3, 143, 0x055F);

}