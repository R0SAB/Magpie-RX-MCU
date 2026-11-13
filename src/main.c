#include <stdio.h>
#include <string.h>
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
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/f1/bkp.h>
#include "buttons.h"
#include <time.h>
#include <stdlib.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/systick.h>
#include "si5351_config.h"

bool boot_flag = 1;
uint32_t freq;                       // Tune frequency in Hz
int32_t correction_ppb;
uint8_t modulation;
uint8_t bandwidth;
uint8_t attenuator;
uint8_t mode;
uint8_t time_field;
enum modulations {MOD_LSB = 0, MOD_USB = 1, MOD_AM = 2};
enum bandwidths {BW_4K8 = 0, BW_2K8 = 1, BW_0K3 = 2};
enum att_values {ATT0, ATT12, ATT24};
enum modes {OPERATION, CORRECTION, VOLUME, TIME_SET, LOCK};
int32_t volume_cnt;
uint8_t volume;
bool battery_depleted;
enum { LOOP_PERIOD_MS = 10, VOL_FADE_MS = 5000 };
/* LCD transfers are asynchronous; shared SPI must wait for DMA as well. */
extern volatile bool dma_busy;
volatile uint32_t millis;
uint32_t millis_mem;
bool shutdown_cancel;
int16_t encoder_delta;

/* Cache rendered text, including its colour. Each slot has fixed coordinates,
 * scale and alignment. Do not use these slots for overlapping screen regions. */
enum text_slots { TEXT_MAIN, TEXT_SMETER, TEXT_TIME, TEXT_DATE,
                  TEXT_VOLTAGE, TEXT_SHUTDOWN, TEXT_SLOT_COUNT };

static void print_changed(enum text_slots slot, uint16_t x, uint16_t y,
                          uint8_t scale, int alignment, const char *text,
                          uint16_t color, uint16_t bg)
{
    static struct {
        bool valid;
        uint16_t color;
        char text[64];
    } cache[TEXT_SLOT_COUNT];

    if(cache[slot].valid && cache[slot].color == color &&
       strcmp(cache[slot].text, text) == 0) return;

    size_t old_length = strlen(cache[slot].text);
    size_t new_length = strlen(text);
    /* lcd_print only paints the new text bounds. Clear the previous bounds
     * when their width changes, including centred strings and disappearing text. */
    if(cache[slot].valid && old_length != new_length && old_length != 0)
    {
        uint16_t width = 6 * scale * old_length;
        uint16_t left = (alignment == ALIGN_CENTER) ? x - width / 2 : x;
        lcd_fill_rect(left, y, width, 7 * scale, bg);
    }
    if(new_length != 0)
        lcd_print(x, y, scale, alignment, (char *)text, color, bg);

    snprintf(cache[slot].text, sizeof(cache[slot].text), "%s", text);
    cache[slot].color = color;
    cache[slot].valid = true;
}

typedef struct {
    uint32_t started;
    bool active;
    bool fired;
} hold_timer;

/* One event per continuous hold, independent of drawing time. */
static bool held_once(hold_timer *timer, bool held, uint32_t now, uint32_t delay)
{
    if(!held)
    {
        timer->active = false;
        timer->fired = false;
        return false;
    }
    if(!timer->active)
    {
        timer->started = now;
        timer->active = true;
    }
    if(!timer->fired && (uint32_t)(now - timer->started) >= delay)
    {
        timer->fired = true;
        return true;
    }
    return false;
}

typedef struct
{
    uint8_t s_meter_byte;
    uint8_t status_byte;
} spi_rx_struct;


const double adc_samp_freq = 70.56e6;
const uint16_t ENCODER_DIV = 2;
const uint32_t ENCODER_FREQ_STEP = 10;

#define FPGA_CS_PORT GPIOA
#define FPGA_CS_PIN GPIO4

#define PWR_HLD_PORT GPIOA
#define PWR_HLD_PIN GPIO1


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

struct packet
{
    uint8_t reg_addr;
    uint8_t buf[64];
};

struct packet i2c_packet;

void i2c_setup()
{

    rcc_periph_clock_enable(RCC_I2C2);
    i2c_peripheral_disable(I2C2);
    i2c_set_clock_frequency(I2C2, 36);
    i2c_set_ccr(I2C2, 180);
    i2c_set_standard_mode(I2C2);
    i2c_set_trise(I2C2, 37);
    i2c_set_own_7bit_slave_address(I2C2, 0b0000111);
    i2c_peripheral_enable(I2C2);
    
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO10 | GPIO11);

}

void si5351_init()
{
    for(uint16_t i = 0; i < SI5351A_REGS_NUM; i++)
    {

        i2c_packet.reg_addr = si5351a_regs[i].address;
        i2c_packet.buf[0] = si5351a_regs[i].value;
        gpio_set(GPIOA, GPIO1);

        i2c_transfer7(I2C2, si5351_addr, &i2c_packet, 2, NULL, 0);
    }
}

void encoder_timer_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8 | GPIO9);

    rcc_periph_clock_enable(RCC_TIM1);
    timer_slave_set_mode(TIM1, TIM_SMCR_SMS_EM3);
    timer_set_prescaler(TIM1, 0);
    timer_enable_counter(TIM1);
}

int16_t encoder_poll(void)
{
    static int16_t encoder_prev;
    static int32_t encoder_accum;

    int16_t encoder_curr = timer_get_counter(TIM1);

    int16_t delta = encoder_curr - encoder_prev;

    encoder_prev = encoder_curr;

    encoder_accum += delta;

    int16_t result = encoder_accum / ENCODER_DIV;

    encoder_accum -= result * ENCODER_DIV;

    return result;
}

void voltmeter_setup(void)
{
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO3);
    rcc_periph_clock_enable(RCC_ADC1);
    adc_power_off(ADC1);
    adc_disable_dma(ADC1);
    adc_disable_scan_mode(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_55DOT5CYC);
    adc_set_regular_sequence(ADC1, 1, (uint8_t[]){ADC_CHANNEL3});
    adc_set_single_conversion_mode(ADC1);
    adc_set_right_aligned(ADC1);
    adc_power_on(ADC1);
    adc_calibrate(ADC1);
}

void voltmeter_routine()
{
    float R_upper = 4.3e3f;
    float R_lower = 2.4e3f;
    float V_ref = 3.3e0f;

    static char voltage_string[16];

    static uint32_t started;
    static bool initialized;
    static bool pending;
    uint32_t now = millis;

    if(!pending && (!initialized || (uint32_t)(now - started) >= 200))
    {
        adc_start_conversion_direct(ADC1);
        started = now;
        initialized = true;
        pending = true;
        return;
    }

    /* Read on a later pass, after the single conversion has completed. */
    if(pending && (uint32_t)(now - started) >= 1)
    {
        pending = false;
        uint16_t adc_value = adc_read_regular(ADC1);

        float volt_float = (float)adc_value / (float)4096 * V_ref * (1.0f + R_upper / R_lower);
        uint16_t V_mv = (uint16_t)(volt_float * 1000);
        uint8_t volt_int = V_mv / 1000;
        uint8_t volt_frac_1 = (V_mv % 1000) / 100;
        uint8_t volt_frac_2 = (V_mv % 100) / 10;
        
        snprintf(voltage_string, sizeof(voltage_string), "Vbat:%d.%d%dV", volt_int, volt_frac_1, volt_frac_2);
        print_changed(TEXT_VOLTAGE, 35, 98, SCALE_1, ALIGN_LEFT, voltage_string, 0x055f, 0x0025);

        if(volt_float < 6.1f) battery_depleted = 1;
        if(volt_float > 6.2f) battery_depleted = 0;
    }
}

void lcd_print_freq_main(uint32_t freq)
{
    uint16_t freq_int = freq/1000;
    uint16_t freq_frac_1 = (freq%1000)/100;
    uint16_t freq_frac_2 = (freq%100)/10;
    

    static char print_buffer[64];
    if(mode == OPERATION || mode == LOCK || mode == VOLUME) snprintf(print_buffer, sizeof(print_buffer), " %d.%d%d kHz ", freq_int, freq_frac_1, freq_frac_2);
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

    print_changed(TEXT_MAIN, 161, 40, SCALE_2, ALIGN_CENTER, print_buffer, 0x055F, 0x0025);
}

void draw_visor(void)
{
    static uint8_t modulation_prev = MOD_AM + 1;
    static uint8_t bandwidth_prev = BW_0K3 + 1;

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

void draw_lock(bool locked)
{
    uint8_t size_x = 18;
    uint8_t size_y = 24;

    uint16_t lock_color = 0xC800;
    uint16_t unlock_color = 0x0660;
    uint16_t bg_color = 0x0025;

    if(locked) lcd_draw_bmp(295, 140, size_x, size_y, bmp_locked, 1, lock_color, bg_color);
    else             lcd_draw_bmp(295, 140, size_x, size_y, bmp_unlocked, 1, unlock_color, bg_color);
}

spi_rx_struct fpga_spi_xfer(uint32_t freq_word, uint8_t modes_byte)
{
        while(dma_busy) {}
        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(LCD_CS_PORT, LCD_CS_PIN);
        spi_set_dff_8bit(LCD_SPI);
        gpio_clear(FPGA_CS_PORT, FPGA_CS_PIN);

        uint8_t status_byte = spi_xfer(LCD_SPI, modes_byte);
        uint8_t s_meter_byte = spi_xfer(LCD_SPI, volume);

        for(int i = 0; i < 4; i++)                              // Send freq word
        {
            while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
            spi_write(LCD_SPI, (freq_word >> (8*(3-i))) & 0xFF);
        }

        while((SPI_SR(LCD_SPI) & SPI_SR_BSY));
        gpio_set(FPGA_CS_PORT, FPGA_CS_PIN);

        spi_rx_struct spi_return;

        spi_return.s_meter_byte = s_meter_byte;
        spi_return.status_byte = status_byte;

        return spi_return;

}

void static_elements_draw(void)
{
    const char* s_meter_nums[9] = {" ", "1", "3", "5", "7", "9", "+12", "+24", "+36"};
    uint16_t nums_x = 15;
    uint16_t nums_x_step = 32;
    uint16_t nums_y = 150;

    lcd_print(nums_x + nums_x_step/2, nums_y, SCALE_1, ALIGN_CENTER, "0", 0x055f, 0x0025);

    for(uint8_t i = 0; i < 9; i++)
    {
        lcd_print(nums_x + nums_x_step*i, nums_y, SCALE_1, ALIGN_CENTER, s_meter_nums[i], 0x055f, 0x0025);
    }

    lcd_print(200, 70, SCALE_1, ALIGN_LEFT, "MOD: LSB USB ", 0x055f, 0x0025);
    lcd_print(200, 85, SCALE_1, ALIGN_LEFT, " BW: 4.8 2.8 ", 0x055f, 0x0025);
    lcd_print(200, 100, SCALE_1, ALIGN_LEFT, "ATT:  0  -12 -24", 0x055f, 0x0025);
    lcd_print(200, 115, SCALE_1, ALIGN_LEFT, "VOL:", 0x055f, 0x0025);

    lcd_fill_rect(230, 116, 3, 6, 0x0e00);      // Volume zero mark

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

void s_meter_print(uint8_t s_value, bool ovr)
{
    char s_value_string[16];

    if(!ovr)
    {
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
    }
    else snprintf(s_value_string, 16, "OVR ");

    uint16_t color;
    if(ovr) color = 0xB211;
    else color = 0x055f;

    print_changed(TEXT_SMETER, 20, 120, SCALE_2, ALIGN_LEFT, s_value_string, color, 0x0025);
}

void s_meter_bar_draw(uint8_t s_value)
{
    static uint8_t previous;
    /* The screen background already represents zero on first draw. */
    if(s_value > 15 || s_value == previous) return;
    uint8_t lower = s_value < previous ? s_value : previous;
    uint8_t upper = s_value > previous ? s_value : previous;
    lcd_fill_rect(31 + 16 * lower, 143, 16 * (upper - lower), 3,
                  s_value > previous ? 0x055f : 0x0025);
    previous = s_value;
}

void lcd_print_time(void)
{
    strftime(time_string, sizeof(time_string), "%H:%M:%S UTC ", &time_struct);
    strftime(date_string, sizeof(date_string), "%d-%m-%Y" , &time_struct);
    print_changed(TEXT_TIME, 20, 70, SCALE_1, ALIGN_LEFT, time_string, 0x055F, 0x0025);
    print_changed(TEXT_DATE, 20, 80, SCALE_1, ALIGN_LEFT, date_string, 0x055F, 0x0025);
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


            BKP_DR5 = (uint16_t)attenuator & 0x00FF;
        }
    }


    if(boot_flag == 1)                              // Initial draw of mode indicators
    {
        modulation = BKP_DR3;
        bandwidth = BKP_DR4;
        attenuator = BKP_DR5;
    }

    uint32_t now = millis;
    static hold_timer att_hold, mod_hold, bw_hold, lock_hold;
    static uint8_t vol_prev_mode;

    if(held_once(&att_hold,
                 (mode == OPERATION || mode == LOCK) && att_btn() == BTN_HLD,
                 now, 350))
    {
        vol_prev_mode = mode;
        mode = VOLUME;
    }
    if(mode == VOLUME && att_btn() == BTN_RLS) mode = vol_prev_mode;

    if(held_once(&mod_hold, mode == OPERATION && mod_btn() == BTN_HLD, now, 1000))
        mode = CORRECTION;
    if(mode == CORRECTION && mod_btn() == BTN_RLS) mode = OPERATION;

    if(held_once(&bw_hold, mode == OPERATION && bw_btn() == BTN_HLD, now, 2000))
    {
        mode = TIME_SET;
        time_field = HOUR;
    }

    if(mode == TIME_SET && bw_btn() == BTN_PRS)     // Exit TIME_SET - single press of BW
    {
        mode = OPERATION;

        timestamp = mktime(&time_struct);
        timestamp_int = (int32_t)timestamp;
        rtc_set_counter_val(timestamp_int);


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

        mktime(&time_struct);
        lcd_print_time();

    }

    if(held_once(&lock_hold, !gpio_get(LOCK_PORT, LOCK_PIN), now, 1000))
    {
        if(mode == OPERATION) mode = LOCK;
        else if(mode == LOCK) mode = OPERATION;
    }

    /* Render only changed visible states. The initial call paints all fields. */
    static bool drawn;
    static uint8_t prev_mod, prev_bw, prev_att, prev_volume;
    static uint8_t prev_time_field;
    static bool prev_blink, prev_locked;
    static uint32_t blink_started;
    static bool blink_phase, was_volume;
    if(mode != VOLUME)
        blink_phase = false;
    else if(!was_volume)
    {
        blink_started = now;
        blink_phase = true;
    }
    else if((uint32_t)(now - blink_started) >= 200)
    {
        blink_started = now;
        blink_phase = !blink_phase;
    }
    was_volume = mode == VOLUME;
    bool blink = blink_phase;
    /* VOLUME temporarily replaces the operating mode, but does not unlock. */
    bool locked = mode == LOCK || (mode == VOLUME && vol_prev_mode == LOCK);
    uint8_t selected_time_field = mode == TIME_SET ? time_field : 0xff;

    if(!drawn || (prev_bw == BW_0K3) != (bandwidth == BW_0K3))
        lcd_print(278, 70, SCALE_1, ALIGN_LEFT, "SAM",
                  bandwidth != BW_0K3 ? 0x055F : 0xB211, bg_color);
    if(!drawn || (prev_mod == MOD_AM) != (modulation == MOD_AM))
        lcd_print(278, 85, SCALE_1, ALIGN_LEFT, "0.3",
                  modulation != MOD_AM ? 0x055F : 0xB211, bg_color);

    for(uint8_t i = 0; i < 3; i++)
    {
        if(!drawn || (prev_mod == i) != (modulation == i))
            lcd_fill_rect(230 + 24*i, 79, 18, 2, modulation == i ? color : bg_color);
        if(!drawn || (prev_bw == i) != (bandwidth == i))
            lcd_fill_rect(230 + 24*i, 94, 18, 2, bandwidth == i ? color : bg_color);
        if(!drawn || (prev_att == i) != (attenuator == i))
            lcd_fill_rect(230 + 24*i, 109, 18, 2, attenuator == i ? color : bg_color);
    }
    if(!drawn || blink != prev_blink)
        lcd_fill_rect(200, 125, 18, 2, blink ? color : bg_color);

    for(uint8_t i = 1; i < 32; i++)
        if(!drawn || (prev_volume >= i) != (volume >= i))
            lcd_fill_rect(232 + 2*i, 116, 1, 6, volume >= i ? 0x055F : bg_color);

    const uint16_t field_x[] = {56, 38, 20, 20, 38, 56};
    const uint16_t field_y[] = {78, 78, 78, 88, 88, 88};
    for(uint8_t i = SEC; i <= YEAR; i++)
        if(!drawn || (prev_time_field == i) != (selected_time_field == i))
            lcd_fill_rect(field_x[i], field_y[i], i == YEAR ? 24 : 12, 1,
                          selected_time_field == i ? color : bg_color);

    if(!drawn || locked != prev_locked) draw_lock(locked);
    prev_mod = modulation;
    prev_bw = bandwidth;
    prev_att = attenuator;
    prev_volume = volume;
    prev_blink = blink;
    prev_locked = locked;
    prev_time_field = selected_time_field;
    drawn = true;
}


bool freq_buttons_polling(void)
{
    if(mode == OPERATION)
    {
        if(plus_100k_btn() == BTN_RLS && !boot_flag)
        {
            uint32_t remainder = freq%100000;
            if(freq > 21900000) freq = 22000000;
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

            if(freq < 600000) freq = 500000;
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
            if(freq > 21000000) freq = 22000000;
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
        if(freq <= 1000000) freq = 500000;
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

void shutdown_timer()
{
    int32_t rtc_curr = rtc_get_counter_val();
    static int32_t rtc_prev;
    static int32_t secs_shutdown;

    if(encoder_delta != 0)
    {
        shutdown_cancel = 1;
        if(pwr_btn() == BTN_HLD) secs_shutdown += encoder_delta;
        if(secs_shutdown < 0) secs_shutdown = 0;
        if(secs_shutdown > 18000) secs_shutdown = 18000;
    }
    else
    if(pwr_btn() == BTN_IDL)
    {
        if(rtc_curr != rtc_prev)
        {
            secs_shutdown--;
            if(secs_shutdown < 60 && secs_shutdown > 1 && volume > 0)
            {
                volume--;
                volume_cnt -= (1 << 6);
            }
        }
    }

    rtc_prev = rtc_curr;

    char print_buffer[20];

    if(secs_shutdown > 0)
    {
        snprintf(print_buffer, sizeof(print_buffer), "OFF: %d min  ", secs_shutdown/60);
    }
    else print_buffer[0] = '\0';
    print_changed(TEXT_SHUTDOWN, 80, 115, SCALE_1, ALIGN_LEFT,
                  print_buffer, 0x055f, 0x0025);

    if(secs_shutdown == 1 && pwr_btn() != BTN_HLD)
    {
        gpio_clear(PWR_HLD_PORT, PWR_HLD_PIN);
        while(1){};
    };

}

void sys_tick_handler(void)
{
    millis++;
}

void main(void){
    
    rcc_clock_setup_in_hsi_out_64mhz();

    systick_set_frequency(1000, rcc_ahb_frequency);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();

    rcc_periph_clock_enable(RCC_AFIO);              // Disable JTAG (to free GPIOs)
    AFIO_MAPR = (uint32_t)(0b010 << 24);
    rcc_periph_clock_disable(RCC_AFIO);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    buttons_setup();

    lcd_dma_setup();
    lcd_init(5);

    lcd_send_cmd_8(0xC3);
    lcd_send_data_8(63);

    lcd_fill_rect(0,0,320,170,0x0025);  // Main background
    lcd_print(161, 80, SCALE_2, ALIGN_CENTER, "Power Up...", 0x055F, 0x0025);
    
    gpio_set_mode(PWR_HLD_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PWR_HLD_PIN);

    while(1)
    {
        buttons_poll();
        
        if(pwr_btn() == BTN_PRS) millis_mem = millis;

        if(pwr_btn() == BTN_HLD)
        {   
            if((millis - millis_mem) > 3000)
            {
                gpio_set(PWR_HLD_PORT, PWR_HLD_PIN);
                break;
            }
        }
    }

    

    //lcd_init(5);
    //lcd_dma_setup();
    encoder_timer_init();
    //buttons_setup();
    rtc_and_bkp_init();
    i2c_setup();
    voltmeter_setup();

    gpio_set_mode(FPGA_CS_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, FPGA_CS_PIN); // FPGA CS pin
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4); // Probe pin
    
    lcd_fill_rect(0,0,320,170,0x0025);  // Main background
    static_elements_draw();                // S meter scale numbers
    
    mode = OPERATION;

    freq = (uint32_t)(BKP_DR2 << 16) | (uint32_t)(BKP_DR1);
    if(freq < 100000 || freq > 28000000) freq = 10000000;

    modulation = BKP_DR3;
    bandwidth = BKP_DR4;
    attenuator = BKP_DR5;
    volume_cnt = 0;

    correction_ppb = (uint32_t)(BKP_DR7 << 16) | (uint32_t)(BKP_DR6);
    if(correction_ppb < -200000) correction_ppb = -200000;
    if(correction_ppb > 200000) correction_ppb = 200000;

    uint64_t ph_acc_fs = (uint64_t) 1 << 32;

    bool flush_scale;

    modes_routine(0x3d40, 0x0025);
    freq_buttons_polling();

    

    si5351_init();

    boot_flag = 0;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);    // ATT 12
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);    // ATT 24

    battery_depleted = 0;

    shutdown_cancel = 1;
    uint32_t last_loop = millis - LOOP_PERIOD_MS;
    uint32_t fade_started = millis;
    bool fade_active = false;
    bool scale_drawn = false;
    uint32_t scale_position_prev = 0;

    while(1)
    {
        uint32_t now = millis;
        if((uint32_t)(now - last_loop) < LOOP_PERIOD_MS) continue;
        last_loop = now;  // No catch-up burst after an overlong pass.

        buttons_poll();
        encoder_delta = encoder_poll();

        if(pwr_btn() == BTN_PRS)
        {
            millis_mem = millis;
            shutdown_cancel = 0;
        }

        if(pwr_btn() == BTN_HLD && !shutdown_cancel)
        {
            if((millis - millis_mem) > 2000)
            {
                lcd_fill_rect(0,0,320,170,0x0025);  // Main background
                lcd_print(161, 80, SCALE_2, ALIGN_CENTER, "Power Down", 0x055F, 0x0025);
                gpio_clear(PWR_HLD_PORT, PWR_HLD_PIN);
                while(1){}
            }
        }

        shutdown_timer();

        if(mode == OPERATION && pwr_btn() == BTN_IDL) freq = freq + encoder_delta * ENCODER_FREQ_STEP;
        else
        if(mode == CORRECTION)
        {
            correction_ppb += encoder_delta;
            BKP_DR6 = (uint16_t)(correction_ppb & 0xFFFF);
            BKP_DR7 = (uint16_t)(correction_ppb >> 16);
        }
        else
        if(mode == VOLUME)
        {
            volume_cnt += encoder_delta * ENCODER_DIV;
            if(volume_cnt > 2047) volume_cnt = 2047;
            if(volume_cnt < 0) volume_cnt = 0;
            volume = volume_cnt >> 6;
        }
        

        if(freq < 500000) freq = 500000;
        if(freq > 22000000) freq = 22000000;

        flush_scale = freq_buttons_polling();
        /* lcd_draw_scale rounds frequency to 125 Hz per pixel. */
        uint32_t scale_position = (freq + 125/2) / 125;
        if(!scale_drawn || scale_position != scale_position_prev || flush_scale)
        {
            lcd_draw_scale(0, 5, 320, 9, freq, flush_scale);
            scale_position_prev = scale_position;
            scale_drawn = true;
        }
        flush_scale = 0;

        draw_visor();

        lcd_print_freq_main(freq);

        double freq_word_float = (double)ph_acc_fs/adc_samp_freq*(double)freq*(double)(1+correction_ppb*1e-9);
        uint32_t freq_word = (uint32_t)freq_word_float;
        
        spi_rx_struct spi_rx;
        uint8_t modes_byte = ((0b11 & bandwidth) << 2) | (0b11 & modulation);
        spi_rx = fpga_spi_xfer(freq_word, modes_byte);
        bool ovr = ((spi_rx.status_byte >> 0) & 0b00000001);
        bool sam_lock = ((spi_rx.status_byte >> 1) & 0b00000001);
        bool corr_err_dir = ((spi_rx.status_byte >> 2) & 0b00000001);

        s_meter_print(spi_rx.s_meter_byte, ovr);
        s_meter_bar_draw(spi_rx.s_meter_byte);

        static bool status_drawn;
        static uint8_t sam_prev, correction_prev;
        uint8_t sam_state = modulation == MOD_AM ? (sam_lock ? 2 : 1) : 0;
        uint8_t correction_state = mode == CORRECTION ? (corr_err_dir ? 2 : 1) : 0;
        if(!status_drawn || sam_state != sam_prev)
            lcd_fill_rect(300, 70, 7, 7,
                          sam_state == 2 ? 0x4ec0 : sam_state == 1 ? 0x5aeb : 0x0025);
        if(!status_drawn || correction_state != correction_prev)
        {
            if(correction_state != 0)
            {
                if(!status_drawn || correction_prev == 0)
                    lcd_fill_rect(160, 68, 3, 7, 0x055f);
                lcd_fill_rect(148, 68, 7, 7, corr_err_dir ? 0x4ec0 : 0x0025);
                lcd_fill_rect(168, 68, 7, 7, corr_err_dir ? 0x0025 : 0x4ec0);
            }
            else lcd_fill_rect(148, 68, 27, 7, 0x0025);
        }
        sam_prev = sam_state;
        correction_prev = correction_state;
        status_drawn = true;

        modes_routine(0x3d40, 0x0025);

        timestamp_int = rtc_get_counter_val();
        if(timestamp_int != timestamp_int_prev && (mode == OPERATION || mode == LOCK))
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

        if(attenuator == ATT12) gpio_set(GPIOB, GPIO12);
        else gpio_clear(GPIOB, GPIO12);

        if(attenuator == ATT24) gpio_set(GPIOA, GPIO12);
        else gpio_clear(GPIOA, GPIO12);

        if(!battery_depleted)
            fade_active = false;
        else if(!fade_active)
        {
            fade_started = now;
            fade_active = true;
        }
        else if((uint32_t)(now - fade_started) >= VOL_FADE_MS)
        {
            fade_started = now;
            if(volume > 0) volume--;
        }

    }

}