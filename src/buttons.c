#include "buttons.h"
#include <libopencm3/stm32/gpio.h>

enum button_indexes {MOD_ID, BW_ID, ATT_ID, M_100K_ID, P_100K_ID, M_1M_ID, P_1M_ID, LOCK_ID, BTN_COUNT};
bool btn_prevs[BTN_COUNT];
bool btn_currs[BTN_COUNT];
uint8_t btn_states[BTN_COUNT];

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

    gpio_set_mode(LOCK_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, LOCK_PIN); // ATT
    gpio_set(LOCK_PORT, LOCK_PIN);

    for(int i = 0; i < BTN_COUNT; ++i)
    {
        btn_prevs[i] = true;
    }
}


void buttons_poll(void)
{
    btn_currs[MOD_ID] = gpio_get(MOD_PORT, MOD_PIN);
    btn_currs[BW_ID] = gpio_get(BW_PORT, BW_PIN);
    btn_currs[ATT_ID] = gpio_get(ATT_PORT, ATT_PIN);
    btn_currs[M_100K_ID] = gpio_get(M_100K_PORT, M_100K_PIN);
    btn_currs[P_100K_ID] = gpio_get(P_100K_PORT, P_100K_PIN);
    btn_currs[M_1M_ID] = gpio_get(M_1M_PORT, M_1M_PIN);
    btn_currs[P_1M_ID] = gpio_get(P_1M_PORT, P_1M_PIN);
    btn_currs[LOCK_ID] = gpio_get(LOCK_PORT, LOCK_PIN);

    for (int i = 0; i < BTN_COUNT; ++i)
    {
        if (!btn_currs[i])
        {
            btn_states[i] = btn_prevs[i] ? BTN_PRS : BTN_HLD;
        }
        else
        {
            btn_states[i] = btn_prevs[i] ? BTN_IDL : BTN_RLS;
        }

        btn_prevs[i] = btn_currs[i];
    }


}


uint8_t plus_100k_btn(void)
{
    return btn_states[P_100K_ID];
}

uint8_t minus_100k_btn(void)
{
    return btn_states[M_100K_ID];
}

uint8_t plus_1M_btn(void)
{
    return btn_states[P_1M_ID];
}

uint8_t minus_1M_btn(void)
{
    return btn_states[M_1M_ID];
}

uint8_t att_btn(void)
{
    return btn_states[ATT_ID];
}

uint8_t mod_btn(void)
{
    return btn_states[MOD_ID];
}

uint8_t bw_btn(void)
{
    return btn_states[BW_ID];
}