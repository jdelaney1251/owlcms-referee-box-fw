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
#include <net/mqtt.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>

#include "comms_mgr.h"
#include "wifi_conn.h"
#include "mqtt_client.h"
#include "settings_util.h"
#include "msys.h"
#include "ble_config_mgr.h"
#include "config_gatt_service.h"

#define SIGNAL_CMD_MAX_RETRIES          10

#define MQTT_CLIENT_NAME_BASE           "owlcms_ref_"

#define DECISION_TOPIC_BASE             "owlcms/decision/"
#define DECISION_GOOD               0
#define DECISION_BAD                1

static const char *decision_msg[] = {"good", "bad"};

#define SUMMON_TOPIC_BASE               "owlcms/summon/"
#define DECISION_REQ_BASE               "owlcms/decisionRequest/"


typedef enum {
    CMD_NONE,
    CMD_RESET,
    CMD_CONNECT,
    CMD_DISCONNECT,
    CMD_MQTT_START,
    CMD_CONFIG_START,
    CMD_CONFIG_STOP
} comms_cmd_t;

static struct k_thread comms_mgr_th;
K_THREAD_STACK_DEFINE(comms_mgr_thread_stack, 4096);
static struct k_msgq  comms_cmd_queue;
char __aligned(1) cmd_msg_buf[10 * sizeof(uint8_t)];
static bool wifi_connected;
static bool net_connected;
static bool mqtt_connected;

struct wifi_config_settings wifi_config;
struct owlcms_config_settings owlcms_config;
struct mqtt_config_settings mqtt_config;
static uint8_t *decision_topic;
static uint8_t decision_topic_len;
static uint8_t ref_number;

static struct k_work_delayable wifi_connect_work;
static struct k_work_delayable wifi_disconnect_work;
static struct k_work_delayable wifi_reset_work;
static struct k_work_delayable wifi_setup_work;

void comms_mgr_thread();
static void process_comms_cmd(comms_cmd_t cmd);
void signal_net_state(uint8_t wifi_state, uint8_t net_state);
void signal_mqtt_state(uint8_t mqtt_state);
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
        wifi_conn_setup(&wifi_config);
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
    // else if (cmd == CMD_CONFIG)
    // {
    //     // TODO: implement configuration mode via BLE
    // }
    else if (cmd == CMD_MQTT_START)
    {
        mqtt_client_setup(&mqtt_config);
        mqtt_client_start();
        //msys_signal_evt(SYS_EVT_CONN_SUCCESS);
    }
    else if (cmd == CMD_CONFIG_START)
    {
        LOG_INF("starting ble config mode");
        mqtt_client_teardown();
        wifi_conn_disconnect();
        struct config_settings settings;
        settings.wifi = &wifi_config;
        settings.mqtt = &mqtt_config;
        settings.owlcms = &owlcms_config;

        ble_config_mgr_start(&settings);
    }
    else if (cmd == CMD_CONFIG_STOP)
    {
        ble_config_mgr_stop();
    }

}

void comms_mgr_thread()
{
    int ret = 0;
    comms_cmd_t cmd = 0;

    while (1)
    {
        ret = k_msgq_get(&comms_cmd_queue, &cmd, K_NO_WAIT);
        if (ret == 0)
        {
            LOG_DBG("comms_mgr evt %d", cmd);
            process_comms_cmd(cmd);
        }
        k_yield();
        //k_msleep(5);
    }
}

int comms_mgr_init(uint8_t device_id)
{
    k_msgq_init(&comms_cmd_queue, cmd_msg_buf, sizeof(uint8_t), 10);
    wifi_connected = false;
    net_connected = false;
    mqtt_connected = false;

    wifi_conn_init();
    mqtt_client_mod_init();

    ref_number = device_id;
    settings_util_load_owlcms_config(&owlcms_config);
    settings_util_load_mqtt_config(&mqtt_config);
    
    uint8_t mqtt_client_name_len = strlen(MQTT_CLIENT_NAME_BASE) + owlcms_config.platform_len + 4;
    snprintk(mqtt_config.client_name, mqtt_client_name_len, "%s%s_%d", MQTT_CLIENT_NAME_BASE,
                                                                        owlcms_config.platform,
                                                                        ref_number);
    mqtt_config.client_name_len = mqtt_client_name_len;


    decision_topic_len = strlen(DECISION_TOPIC_BASE) + owlcms_config.platform_len + 1;
    decision_topic = k_malloc(sizeof(uint8_t) * decision_topic_len);
    snprintk(decision_topic, decision_topic_len, "%s%s", DECISION_TOPIC_BASE, owlcms_config.platform);
    
    settings_util_load_wifi_config(&wifi_config);
    LOG_DBG("loaded wifi ssid/psk, %s, %s", wifi_config.ssid, wifi_config.psk);

    wifi_conn_set_net_state_cb(&signal_net_state);
    mqtt_client_set_state_cb(&signal_mqtt_state);

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
    //LOG_DBG("comms_mgr evt %d", cmd);
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

int comms_mgr_disconnect()
{
    return comms_mgr_signal_cmd(CMD_DISCONNECT);
}

int comms_mgr_notify_decision(uint8_t decision)
{
    uint8_t ret = 0;
    uint8_t *msg;
    
    
    uint8_t msg_len = strlen(decision_msg[decision]) + 3; // plus 3 for space, ID nummber, \0
    msg = k_malloc(sizeof(uint8_t) * msg_len);
    snprintk(msg, msg_len, "%d %s", ref_number, decision_msg[decision]);
    LOG_DBG("notify decision rx, topic: %s %d, msg: %s %d", decision_topic, decision_topic_len, msg, msg_len);
    ret = mqtt_client_publish(MQTT_QOS_0_AT_MOST_ONCE, decision_topic, decision_topic_len-1, msg, msg_len-1);

    if (ret == 0)
        msys_signal_evt(SYS_EVT_DECISION_HANDLED);

    return ret;
}

int comms_mgr_start_config()
{
    comms_mgr_signal_cmd(CMD_CONFIG_START);
}

int comms_mgr_end_config()
{
    comms_mgr_signal_cmd(CMD_CONFIG_STOP);
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
        comms_mgr_signal_cmd(CMD_MQTT_START);
    }
    else if (wifi_state == WIFI_CONN_STATE_DOWN && net_state == WIFI_CONN_STATE_DOWN)
    {
        wifi_connected = false;
        net_connected = false;
        mqtt_client_teardown();
        msys_signal_evt(SYS_EVT_CONN_LOST);
    }
}

void signal_mqtt_state(uint8_t mqtt_state)
{
    if (mqtt_state == MQTT_STATE_NO_CHANGE)
    {

    }
    else if (mqtt_state == MQTT_STATE_CONNECTED)
    {
        msys_signal_evt(SYS_EVT_CONN_SUCCESS);
    }
    else if (mqtt_state == MQTT_STATE_DISCONNECTED)
    {
        // TODO: handle mqtt connection loss
    }
}