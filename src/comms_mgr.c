#include <logging/log.h>

LOG_MODULE_REGISTER(comms_mgr, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/wifi_mgmt.h>
#include <net/net_mgmt.h>
#include <net/ethernet_mgmt.h>
#include <net/wifi.h>
#include <net/net_event.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>

#include "comms_mgr.h"
#include "wifi_conn.h"
#include "mqtt_client.h"
#include "msys.h"

#define SIGNAL_CMD_MAX_RETRIES          10

typedef enum {
    CMD_NONE,
    CMD_RESET,
    CMD_CONNECT,
    CMD_DISCONNECT,
    CMD_CONFIG
} comms_cmd_t;

static struct k_thread comms_mgr_th;
K_THREAD_STACK_DEFINE(comms_mgr_thread_stack, 4096);
static struct k_msgq  comms_cmd_queue;
char __aligned(1) cmd_msg_buf[10 * sizeof(uint8_t)];
static bool wifi_connected;
static bool net_connected;
static bool mqtt_connected;

static struct k_work_delayable wifi_connect_work;
static struct k_work_delayable wifi_disconnect_work;
static struct k_work_delayable wifi_reset_work;
static struct k_work_delayable wifi_setup_work;

void comms_mgr_thread();
static void process_comms_cmd(comms_cmd_t cmd);
void signal_net_state(uint8_t wifi_state, uint8_t net_state);
static int comms_mgr_signal_cmd(comms_cmd_t cmd);

static void process_comms_cmd(comms_cmd_t cmd)
{
    if (cmd == CMD_RESET)
    {
        LOG_INF("proc cmd RESET");
        wifi_conn_disconnect();
        wifi_conn_reset();
        // k_work_reschedule(&wifi_disconnect_work, K_NO_WAIT);
        // k_work_reschedule(&wifi_reset_work, K_NO_WAIT);
    }
    else if (cmd == CMD_CONNECT)
    {
        LOG_INF("proc cmd CONNECT");

        wifi_conn_reset();
        wifi_conn_setup();
        wifi_conn_connect();
        // k_work_reschedule(&wifi_reset_work, K_NO_WAIT);
        // k_work_reschedule(&wifi_setup_work, K_NO_WAIT);
        // k_work_reschedule(&wifi_connect_work, K_NO_WAIT);
    }
    else if (cmd == CMD_DISCONNECT)
    {
        LOG_INF("proc cmd DISCONNECT");
        wifi_conn_disconnect();
        // k_work_reschedule(&wifi_disconnect_work, K_NO_WAIT);
    }
    else if (cmd == CMD_CONFIG)
    {
        // TODO: implement configuration mode via BLE
    }

}

void comms_mgr_thread()
{
    int ret = 0;
    comms_cmd_t cmd;

    while (1)
    {
        ret = k_msgq_get(&comms_cmd_queue, &cmd, K_NO_WAIT);
        if (ret == 0)
        {
            process_comms_cmd(cmd);
        }
        k_yield();
    }
}

int comms_mgr_init()
{
    k_msgq_init(&comms_cmd_queue, cmd_msg_buf, sizeof(uint8_t), 10);
    wifi_connected = false;
    net_connected = false;
    mqtt_connected = false;

    wifi_conn_init();
    mqtt_client_mod_init();
    
    wifi_conn_set_net_state_cb(&signal_net_state);

    k_work_init_delayable(&wifi_connect_work, wifi_conn_connect);
    k_work_init_delayable(&wifi_disconnect_work, wifi_conn_disconnect);
    k_work_init_delayable(&wifi_reset_work, wifi_conn_reset);
    //k_work_init_delayable(&wifi_configure_work, wifi_configure);
    k_thread_create(&comms_mgr_th,
                    comms_mgr_thread_stack,
                    K_THREAD_STACK_SIZEOF(comms_mgr_thread_stack),
                    comms_mgr_thread,
                    NULL, NULL, NULL, 
                    6, 0, K_NO_WAIT);

    return 0;
}

int comms_mgr_signal_cmd(comms_cmd_t cmd)
{
    comms_cmd_t c = cmd;
    uint8_t count = 0;
    LOG_DBG("comms_mgr evt %d", cmd);
    while (k_msgq_put(&comms_cmd_queue, &c, K_NO_WAIT) != 0)
    {
        if (count >= SIGNAL_CMD_MAX_RETRIES)
        {
            LOG_DBG("Failed to put cmd on msgq");
            return -ETIMEDOUT;
        }
        count++;
    }
    return 0;
}

int comms_mgr_connect()
{
    return comms_mgr_signal_cmd(CMD_CONNECT);
}

void signal_net_state(uint8_t wifi_state, uint8_t net_state)
{
    if (wifi_state == WIFI_CONN_STATE_UP)
    {
        wifi_connected = true;
    }
    else if (net_state == WIFI_CONN_STATE_UP)
    {
        net_connected = true;
        mqtt_client_setup();
        mqtt_client_start();
        msys_signal_evt(SYS_EVT_CONN_SUCCESS);
    }
    else if (wifi_state == WIFI_CONN_STATE_DOWN && net_state == WIFI_CONN_STATE_DOWN)
    {
        wifi_connected = false;
        net_connected = false;
        mqtt_client_teardown();
        msys_signal_evt(SYS_EVT_CONN_LOST);
    }
}