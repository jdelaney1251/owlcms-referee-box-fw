#include <logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log_ctrl.h>

#include "io.h"
#include "msys.h"
#include "comms_mgr.h"
#include "settings_util.h"

void main(void)
{
    log_init();
    LOG_INF("Starting OWLCMS Referee Controller...\n");

    // Short startup delay, don't think this is really necessary
    k_sleep(K_MSEC(100));

    int ret = 0;
    ret = io_init();
    if (ret != 0)
    {
        LOG_ERR("Failed to initialise IO module");
        return;
    }

    ret = settings_util_init();
    if (ret != 0)
    {
        LOG_ERR("Failed to initialise NVS and settings module");
        return;
    }

    ret = comms_mgr_init(io_get_dev_id());
    if (ret != 0)
    {
        LOG_ERR("Failed to initialise comms module");
        return;
    }

    ret = msys_init();
    if (ret != 0)
    {
        LOG_ERR("Failed to initialise main system module");
        return;
    }

    ret = msys_run();
    if (ret != 0)
    {
        LOG_ERR("Failed to start main system module");
        return;
    }

    while(1)
    {
        // Process buffered log messages and sleep;
        if (log_process() == false)
            k_sleep(K_MSEC(100));
    }
    // never going to get here...
    LOG_INF("main thread exiting");
}

