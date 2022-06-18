#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mqtt_client_mod, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>

#include "settings_util.h"
#include "mqtt_client.h"

#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE        1024
#endif

#ifndef MQTT_CONNECT_TIMEOUT_MS
#define MQTT_CONNECT_TIMEOUT_MS 5000
#endif

#ifndef MQTT_CLIENT_KEEPALIVE_MS
#define MQTT_CLIENT_KEEPALIVE_MS 600000  // 10 mins keepalive timeout
#endif

#ifndef MQTT_CLIENT_PING_TIMEOUT
#define MQTT_CLIENT_PING_TIMEOUT    MQTT_CLIENT_KEEPALIVE_MS - 30000      // make it slightly before the timeout
#endif

#ifndef MQTT_CLIENT_STACKSIZE
#define MQTT_CLIENT_STACKSIZE   2048
#endif

#ifndef MQTT_CLIENT_MAX_CONNECT_RETRIES
#define MQTT_CLIENT_MAX_CONNECT_RETRIES     5
#endif

#ifndef MQTT_CLIENT_CONNECT_TIMEOUT_MS
#define MQTT_CLIENT_CONNECT_TIMEOUT_MS  2000
#endif

#define MQTT_CLIENT_INPUT_TIMEOUT_MS            5

static uint8_t rx_buffer[MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_BUFFER_SIZE];

static bool running;
static bool connected;

static struct sockaddr_storage broker_serv;

static struct mqtt_client client;
static struct mqtt_config_settings config;

static struct zsock_pollfd fds[1];

struct k_thread *mqtt_client_handle;
struct k_work_delayable mqtt_client_live_work;
struct k_work_delayable mqtt_client_input_work;

static void mqtt_client_live();
static void mqtt_client_input(bool reschedule);
static void mqtt_client_thread();
static void setup_socket_fds();

void mqtt_evt_handler(struct mqtt_client *client, 
                        const struct mqtt_evt *evt)
{
    switch (evt->type)
    {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0)
        {
            LOG_ERR("mqtt connect failed");
            break;
        }

        connected = true;
        LOG_INF("mqtt client connected");
        break;
    case MQTT_EVT_DISCONNECT:
        LOG_INF("mqtt client disconnected");

        connected = false;
        break;

    case MQTT_EVT_PINGRESP:
        LOG_INF("pingresp received");
        k_work_schedule(&mqtt_client_live_work, K_MSEC(MQTT_CLIENT_PING_TIMEOUT));
    default:
        LOG_INF("mqtt evt: %d", evt->type);
        break;
    }
}

static void setup_socket_fds()
{
    if (client.transport.type == MQTT_TRANSPORT_NON_SECURE)
    {
        fds[0].fd = client.transport.tcp.sock;
        fds[0].events = ZSOCK_POLLIN;
    }
}

int mqtt_client_mod_init()
{
    connected = false;

    k_work_init_delayable(&mqtt_client_live_work, mqtt_client_live);
    k_work_init_delayable(&mqtt_client_input_work, mqtt_client_input);
}

int mqtt_client_setup()
{
    

    settings_util_load_mqtt_config(&config);

    LOG_INF("starting");
    mqtt_client_init(&client);
    LOG_INF("setting up client addr");
    struct sockaddr_in *broker = (struct sockaddr_in *)&broker_serv;
    broker->sin_family = AF_INET;
    broker->sin_port = htons(config.port);
    char *serv_str = k_malloc(sizeof(char) * config.broker_addr_len);
    snprintk(serv_str, config.broker_addr_len, "%s", config.broker_addr);
    zsock_inet_pton(AF_INET, serv_str, &broker->sin_addr);
    
    LOG_INF("cfg client params");
    client.broker = &broker_serv;
    client.evt_cb = mqtt_evt_handler;
    client.client_id.utf8 = config.client_name;
    client.client_id.size = config.client_name_len;
    client.password = NULL;
    client.user_name = NULL;
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    client.keepalive = MQTT_CLIENT_KEEPALIVE_MS/1000;

    client.rx_buf = rx_buffer;
    client.tx_buf = tx_buffer;
    client.rx_buf_size = MQTT_BUFFER_SIZE;
    client.tx_buf_size = MQTT_BUFFER_SIZE;

    LOG_INF("done");

    return 0;
}

int mqtt_client_start()
{
    int ret = 0;
    int retries = 0;

    while (retries++ < MQTT_CLIENT_MAX_CONNECT_RETRIES && !connected)
    {
        LOG_INF("trying connect");
        ret = mqtt_connect(&client);
        if (ret != 0)
        {
            LOG_ERR("failed to connect, trying again... (%d)", ret);
            continue;
        }

        setup_socket_fds();
        zsock_poll(fds, 1, MQTT_CLIENT_CONNECT_TIMEOUT_MS);
        mqtt_client_input(false);

        if (!connected)
        {
            LOG_ERR("connect failed, aborting...");
            mqtt_client_teardown();
        }
    }

    if (connected)
    {
        mqtt_client_live();
    }

    return 0;
}

int mqtt_client_teardown()
{
    // to be used when network conn is lost and we can't disconnect gracefully
    // cancel any work queues
    k_work_cancel_delayable(&mqtt_client_live_work);
    k_work_cancel_delayable(&mqtt_client_input_work);
    // call mqtt_abort
    mqtt_abort(&client);

    return 0;
}

static void mqtt_client_live()
{
    int ret = 0;
    ret = mqtt_live(&client);

    if (ret != 0)
    {
        LOG_ERR("live failed %d", ret);
    }

    //k_work_schedule(&mqtt_client_live_work, MQTT_CLIENT_PING_TIMEOUT);
}

static void mqtt_client_input(bool reschedule)
{
    int ret = 0;
    ret = mqtt_input(&client);

    if (ret != 0)
    {
        LOG_ERR("input failed %d", ret);
    }

    if (reschedule)
        k_work_schedule(&mqtt_client_input_work, K_MSEC(MQTT_CLIENT_INPUT_TIMEOUT_MS));
}