#include "bmp.h"

#include "lib_7789.h"

void main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(5);
 
    lcd_fill_rect(0,0,320,170,0x0025);

    lcd_print(4, 10, 1, "Rito", 0x055F, 0x0025);
    lcd_print(4, 20, 2, "Saki", 0x055F, 0x0025);
    lcd_print(4, 40, 3, "Teba", 0x055F, 0x0025);
    lcd_print(4, 70, 4, "Revali", 0x055F, 0x0025);
    lcd_print(4, 110, 5, "Hyzeth", 0x055F, 0x0025);

    lcd_draw_bmp(140, 0, 179, 100, bitmap);

    lcd_draw_rect(10,10,70,100,5,0x0FFF);

    lcd_draw_rect(5,5,310,160,2,0xFFFF);
}