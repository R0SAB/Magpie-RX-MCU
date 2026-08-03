#pragma once
#include <stdint.h>

#define P_100K_PORT GPIOB
#define P_100K_PIN GPIO6
#define M_100K_PORT GPIOB
#define M_100K_PIN GPIO5
#define P_1M_PORT GPIOB
#define P_1M_PIN GPIO8
#define M_1M_PORT GPIOB
#define M_1M_PIN GPIO7
#define BW_PORT GPIOA
#define BW_PIN GPIO15
#define MOD_PORT GPIOB
#define MOD_PIN GPIO3
#define ATT_PORT GPIOB
#define ATT_PIN GPIO4

#define LOCK_PORT GPIOB
#define LOCK_PIN GPIO9

void buttons_setup(void);
uint8_t plus_100k_btn(void);
uint8_t minus_100k_btn(void);
uint8_t plus_1M_btn(void);
uint8_t minus_1M_btn(void);
uint8_t bw_btn(void);
uint8_t mod_btn(void);
uint8_t att_btn(void);

enum button_states {BTN_IDL, BTN_PRS, BTN_RLS, BTN_HLD}; // Idle, Pressed, Released, Held