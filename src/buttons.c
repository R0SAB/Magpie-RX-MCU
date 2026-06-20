#include "buttons.h"
#include <libopencm3/stm32/gpio.h>

void buttons_setup(void)
{
    gpio_set_mode(P_100K_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, P_100K_PIN); // +1M
    gpio_set(P_100K_PORT, P_100K_PIN);
    gpio_set_mode(M_100K_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, M_100K_PIN); // -1M
    gpio_set(M_100K_PORT, M_100K_PIN);
    gpio_set_mode(P_1M_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, P_1M_PIN); // +100k
    gpio_set(P_1M_PORT, P_1M_PIN);
    gpio_set_mode(M_1M_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, M_1M_PIN); // -100k
    gpio_set(M_1M_PORT, M_1M_PIN);
    gpio_set_mode(BW_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, BW_PIN); // BW
    gpio_set(BW_PORT, BW_PIN);
    gpio_set_mode(MOD_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, MOD_PIN); // MOD
    gpio_set(MOD_PORT, MOD_PIN);
    gpio_set_mode(ATT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, ATT_PIN); // ATT
    gpio_set(ATT_PORT, ATT_PIN);
}

uint8_t plus_100k_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(P_100K_PORT, P_100K_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t minus_100k_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(M_100K_PORT, M_100K_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t plus_1M_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(P_1M_PORT, P_1M_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t minus_1M_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(M_1M_PORT, M_1M_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t att_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(ATT_PORT, ATT_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t mod_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(MOD_PORT, MOD_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}

uint8_t bw_btn(void)
{
    uint8_t state;
    static bool prev;
    bool curr = gpio_get(BW_PORT, BW_PIN);

    if(curr == 0 && prev == 0) state = BTN_HLD;
    else
    if(curr == 0 && prev == 1) state = BTN_PRS;
    else
    if(curr == 1 && prev == 0) state = BTN_RLS;
    else
    if(curr == 1 && prev == 1) state = BTN_IDL;

    prev = curr;
    return state;
}