#include "lib_7789.h"
#include "bmp.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/spi.h>
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

void main(void){
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(6);

    lcd_dma_setup();

    encoder_timer_init();
 
    lcd_fill_rect(0,0,320,170,0x0025);

    uint16_t encoder_curr = 0;
    uint16_t encoder_prev = 0;
    int16_t encoder_diff = 0;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);

    uint32_t freq = 10000000;

    while(1)
    {

        freq = freq + encoder_delta()*5;

        lcd_draw_line(160, 5, 160, 9, 0xe8c3);
        lcd_draw_line(160, 20, 160, 24, 0xe8c3);
        lcd_draw_scale(0, 10, 320, 9, freq);

        //gpio_set(GPIOA, GPIO4);
        
        lcd_draw_freq_main(freq);

        //gpio_clear(GPIOA, GPIO4);

    }

}