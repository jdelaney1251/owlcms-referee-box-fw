#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mqtt_client_mod, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>

#include "settings_util.h"
#include "mqtt_client.h"

#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE        256
#endif

#ifndef MQTT_CONNECT_TIMEOUT_MS
#define MQTT_CONNECT_TIMEOUT_MS 5000
#endif

#ifndef MQTT_CLIENT_KEEPALIVE_MS
#define MQTT_CLIENT_KEEPALIVE_MS 60000  // 1 min keepalive timeout
#endif

#ifndef MQTT_CLIENT_PING_TIMEOUT
#define MQTT_CLIENT_PING_TIMEOUT    MQTT_CLIENT_KEEPALIVE_MS      // make it slightly before the timeout
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

#define MQTT_CLIENT_INPUT_TIMEOUT_MS            100

#define MQTT_CLIENT_TICK_PERIOD                 10000

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

struct k_thread mqtt_client_th;
K_THREAD_STACK_DEFINE(mqtt_client_th_stack, 2096);

static void mqtt_client_live();
static void mqtt_client_input();
static void mqtt_client_thread();
static void setup_socket_fds();

void mqtt_client_set_state_cb(void (*cb)(uint8_t mqtt_state));

static void (*mqtt_state_cb)(uint8_t mqtt_state);

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
        mqtt_state_cb(MQTT_STATE_CONNECTED);
        LOG_INF("mqtt client connected");
        break;
    case MQTT_EVT_DISCONNECT:
        LOG_INF("mqtt client disconnected");
        mqtt_state_cb(MQTT_STATE_DISCONNECTED);
        connected = false;
        break;

    case MQTT_EVT_PINGRESP:
        LOG_INF("pingresp received");
        //k_work_schedule(&mqtt_client_live_work, K_MSEC(MQTT_CLIENT_PING_TIMEOUT));
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
    //broker->sin_port = htons(config.port);
    broker->sin_port = htons(1883);
    //char *serv_str = k_malloc(sizeof(char) * config.broker_addr_len);
    //snprintk(serv_str, config.broker_addr_len, "%s", config.broker_addr);
    zsock_inet_pton(AF_INET, "10.0.0.7", &broker->sin_addr);
    
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

    running = true;
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
        LOG_INF("setting up socket");
        setup_socket_fds();
        LOG_INF("poll socket");
        zsock_poll(fds, 1, MQTT_CLIENT_CONNECT_TIMEOUT_MS);
        LOG_INF("mqtt input");
        mqtt_input(&client);

        if (!connected)
        {
            LOG_ERR("connect failed, aborting...");
            mqtt_client_teardown();
        }
    }

    if (connected)
    {
        //k_work_schedule(&mqtt_client_live_work, K_MSEC(MQTT_CLIENT_PING_TIMEOUT));

        k_thread_create(&mqtt_client_th, mqtt_client_th_stack,
                        K_THREAD_STACK_SIZEOF(mqtt_client_th_stack),
                        mqtt_client_thread,
                        NULL, NULL, NULL,
                        6, 0, K_NO_WAIT);
    }

    return 0;
}

static void mqtt_client_thread()
{
    LOG_INF("starting mqtt client thread");
    uint8_t ret = 0;

    while (connected)
    {
        zsock_poll(fds, 1, MQTT_CLIENT_CONNECT_TIMEOUT_MS);
        ret = mqtt_input(&client);
        if (ret != 0) 
        {
            LOG_ERR("mqtt input err: %d", ret);
            return;
        }

        ret = mqtt_live(&client);

        //k_sleep(K_MSEC(MQTT_CLIENT_TICK_PERIOD));
    }

    if (!connected)
    {
        LOG_INF("maint thread exiting");
    }
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
    LOG_DBG("mqtt client keepalive refresh");
    int ret = 0;
    ret = mqtt_live(&client);

    if (ret != 0 && ret != -EAGAIN)
    {
        LOG_ERR("live failed %d", ret);
        k_work_schedule(&mqtt_client_live_work, K_MSEC(5000));
        return;
    }
    else if (ret == 0)
    {
        ret = mqtt_input(&client);
    }

}

static void mqtt_client_input()
{
    int ret = 0;
    ret = mqtt_input(&client);

    if (ret != 0)
    {
        LOG_ERR("input failed %d", ret);
    }

    //k_work_schedule(&mqtt_client_input_work, K_MSEC(MQTT_CLIENT_INPUT_TIMEOUT_MS));
}

int mqtt_client_publish(   enum mqtt_qos qos, 
                            uint8_t *topic,
                            uint32_t topic_len,
                            uint8_t *data, 
                            uint32_t data_len)
{
    if (running && connected)
    {
        struct mqtt_publish_param param;

        param.message.topic.qos = qos;
        param.message.topic.topic.utf8 = topic;   
        param.message.topic.topic.size = topic_len;
        param.message.payload.data = data;
        param.message.payload.len = data_len;
        param.message_id = 1;
        param.dup_flag = 0;
        param.retain_flag = 0;

        return mqtt_publish(&client, &param);
    }
    return -EINVAL;
}

void mqtt_client_set_state_cb(void (*cb)(uint8_t mqtt_state))
{
    mqtt_state_cb = cb;
}