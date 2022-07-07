#include <logging/log.h>

LOG_MODULE_REGISTER(io_mgr, LOG_LEVEL_DBG);

#include <zephyr.h>

#include "io_mgr.h"
#include "io.h"
#include "msys.h"

#define BUZZER_ON_PERIOD_MS            1000

static struct k_work_delayable leds_disable_work;

static struct k_work_delayable buzzer_off_work;

static const leds_cfg_t leds_cfg_connecting = {
    .cfg_type = LED_CFG_TYPE_BLINK_ALT,
    .pattern = 0x0A
};

static const leds_cfg_t leds_cfg_bat_level = {
    .cfg_type = LED_CFG_TYPE_ON_OFF,
    .pattern = 0x00
};

static const leds_cfg_t leds_cfg_disable = {
    .cfg_type = LED_CFG_TYPE_DISABLE,
    .pattern = 0x00
};

static const leds_cfg_t leds_cfg_config = {
    .cfg_type = LED_CFG_TYPE_BLINK_STATIC,
    .pattern = 0x01
};

void btn_usr_handler(uint8_t evt_type);
void btn_red_handler(uint8_t evt_type);
void btn_blk_handler(uint8_t evt_type);

static void leds_disable_work_fn(struct k_work *work)
{
    io_mgr_set_leds_disable();
}

static void buzzer_off_work_fn(struct k_work *work)
{
    io_buzzer_off();
}

int io_mgr_init()
{
    LOG_DBG("init...");
    io_reg_cb_btn_usr(&btn_usr_handler);
    io_reg_cb_btn_red(&btn_red_handler);
    io_reg_cb_btn_blk(&btn_blk_handler);

    k_work_init_delayable(&leds_disable_work, leds_disable_work_fn);
    k_work_init_delayable(&buzzer_off_work, buzzer_off_work_fn);
}

int io_mgr_set_leds_config()
{
    return io_set_leds_cfg(leds_cfg_config);
}

int io_mgr_set_leds_connecting()
{
    return io_set_leds_cfg(leds_cfg_connecting);
}

int io_mgr_set_leds_bat_level()
{
    // read from adc
    // convert adc to 4-step level
    // display level using LEDs on/off pattern
    leds_cfg_t cfg = leds_cfg_bat_level;
    cfg.pattern = 0x0F;
    io_set_leds_cfg(cfg);

    // set timeout cb to trigger LEDs to turn off after a short time
    k_work_reschedule(&leds_disable_work, K_MSEC(2000));
}

int io_mgr_set_leds_disable()
{
    return io_set_leds_cfg(leds_cfg_disable);
}

int io_mgr_buzzer_trig()
{
    io_buzzer_on();

    k_work_reschedule(&buzzer_off_work, K_MSEC(BUZZER_ON_PERIOD_MS));
}

void btn_usr_handler(uint8_t evt_type)
{
    if (evt_type == BTN_EVT_PRESSED)
    {
        //io_mgr_set_leds_bat_level();
        msys_signal_evt(SYS_EVT_CONFIG_END);
        
    }
    else if (evt_type == BTN_EVT_HOLD_2s)
    {
        msys_signal_evt(SYS_EVT_CONFIG);
    }
}

void btn_red_handler(uint8_t evt_type)
{
    if (evt_type == BTN_EVT_PRESSED)
    {
        msys_signal_evt(SYS_EVT_INP_RED_DECISION);
    }
    else if (evt_type == BTN_EVT_HOLD_2s)
    {
        msys_signal_evt(SYS_EVT_CONN_LOST);
    }
}

void btn_blk_handler(uint8_t evt_type)
{
    if (evt_type == BTN_EVT_PRESSED)
    {
        msys_signal_evt(SYS_EVT_INP_BLK_DECISION);
    }
    else if (evt_type == BTN_EVT_HOLD_2s)
    {
        msys_signal_evt(SYS_EVT_CONN_SUCCESS);
    }
}
