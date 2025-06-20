#include "lib_7789.h"
#include "bmp.h"


void main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    lcd_init(5);
 
    lcd_fill_rect(0,0,320,170,0x0025);

    lcd_print(45, 12, 2, "Hyzeth", 0x055F, 0x0025);

    lcd_draw_bmp(160, 10, 150, 150, bitmap);

    lcd_draw_rect(155, 5, 160,160 ,2,0x055F);

    lcd_print(5, 35, 1,  "Race: Rito", 0x055F, 0x0025);
    lcd_print(5, 45, 1,  "Gender: male", 0x055F, 0x0025);
    lcd_print(5, 55, 1,  "Home: Rito Village", 0x055F, 0x0025);
    lcd_print(5, 65, 1,  "Personality: gentle,", 0x055F, 0x0025);
    lcd_print(5, 75, 1,  "      careful, calm, sad", 0x055F, 0x0025);
    lcd_print(5, 85, 1,  "Occupation: ancient tech", 0x055F, 0x0025);
    lcd_print(5, 95, 1, "      (Sheikah and Zonai)", 0x055F, 0x0025);
    lcd_print(5, 105, 1, "Hobby: music (Hammond", 0x055F, 0x0025);
    lcd_print(5, 115, 1, "               organ)", 0x055F, 0x0025);
    lcd_print(5, 125, 1, "Special traits: mute", 0x055F, 0x0025);
    lcd_print(5, 135, 1, "Other: sheltered some", 0x055F, 0x0025);
    lcd_print(5, 145, 1, "      hylian, with which", 0x055F, 0x0025);
    lcd_print(5, 155, 1, "      is in relationship", 0x055F, 0x0025);

}