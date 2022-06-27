#ifndef IO_H_ 
#define IO_H_

#include <stdint.h>


#define BTN_EVT_PRESSED             0
#define BTN_EVT_RELEASED            1
#define BTN_EVT_HOLD_2s             2
#define BTN_EVT_HOLD_5s             3

#define LED_CFG_TYPE_DISABLE        0
#define LED_CFG_TYPE_ON_OFF         1   // turn leds on/off according to pattern bits
#define LED_CFG_TYPE_BLINK_STATIC   2   // blink led bits set to 1 in pattern
#define LED_CFG_TYPE_BLINK_ALT      3   // blink led bits set to 1/0 in pattern alternatively

#define BZR_CFG_TYPE_DISABLE        0
#define BZR_CFG_TYPE_ON_OFF         1   // buzzer on/off (ignore time field)
#define BZR_CFG_TYPE_TIMEOUT        2   // buzzer on for time specified
#define BZR_CFG_TYPE_PERIODIC       3   // buzzer on/off at specified time intervals

typedef struct btn_evt {
    uint8_t btn_id;
    uint8_t evt_type;
} btn_evt_t;

typedef struct leds_cfg {
    uint8_t cfg_type;
    uint8_t pattern;
} leds_cfg_t;

typedef struct buzzer_cfg {
    uint8_t cfg_type;
    uint8_t time_ms;
} buzzer_cfg_t;

int io_init();

int io_set_leds_cfg(leds_cfg_t cfg);
int io_set_cfg_buzzer(buzzer_cfg_t cfg);

int io_set_led(uint8_t led);
int io_clr_led(uint8_t led);

int io_buzzer_on();
int io_buzzer_off();

void io_reg_cb_btn_usr(void (*cb)(uint8_t));
void io_reg_cb_btn_red(void (*cb)(uint8_t));
void io_reg_cb_btn_blk(void (*cb)(uint8_t));

int io_get_red_btn_state();

int io_get_dev_id();

void btn_evt_thread(void *unused, void *unused1, void *unused2);

#endif