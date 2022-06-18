#include <logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <net/wifi_mgmt.h>

#include "io.h"
#include "msys.h"
#include "comms_mgr.h"
#include "settings_util.h"

static struct k_thread main_th;



void main(void)
{
    LOG_INF("Starting OWLCMS Referee Controller...\n");

    k_sleep(K_MSEC(1000));
    int ret = 0;
    ret = io_init();
    ret = settings_util_init();
    ret = comms_mgr_init();

    ret = msys_init();
    msys_run();

    leds_cfg_t leds_cfg;

    // leds_cfg.cfg_type = LED_CFG_TYPE_BLINK_ALT;
    // leds_cfg.pattern = 0x0A;
    // io_set_leds_cfg(leds_cfg);

    LOG_INF("ID of this device is %d", io_get_dev_id());

    /*
    struct wifi_connect_req_params wifi_cfg;
    wifi_cfg.ssid_length = 10;
    settings_util_init();
    settings_util_load_wifi_config(&wifi_cfg);
    LOG_INF("ssid len %d", wifi_cfg.ssid_length);

    settings_util_set_wifi_ssid(NULL, 5);
    settings_util_load_wifi_config(&wifi_cfg);
    LOG_INF("ssid len %d", wifi_cfg.ssid_length);
    */
   
   /*
    struct wifi_connect_req_params params;
    
    settings_util_load_wifi_config(&params);
    settings_util_set_wifi_ssid("Delaney", 7);


    settings_util_load_wifi_config(&params);
    settings_util_set_wifi_ssid("NotDelaney", 10);
    settings_util_load_wifi_config(&params);
    */

    while(1)
    {
        //printk("tick\n");
        k_sleep(K_MSEC(1000));
    }
    LOG_INF("main thread exiting");
}

