#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_mod, LOG_LEVEL_DBG);

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

#include "wifi_conn.h"

#define IF_MGMT_EVENTS (NET_EVENT_IF_UP                    | \
                        NET_EVENT_IF_DOWN)

#define ETH_MGMT_EVENTS (NET_EVENT_ETHERNET_CARRIER_ON      | \
                         NET_EVENT_ETHERNET_CARRIER_OFF)

#define IP_MGMT_EVENTS (NET_EVENT_IPV4_DHCP_BOUND)

static struct net_mgmt_event_callback if_mgmt_cb;
static struct net_mgmt_event_callback eth_mgmt_cb;
static struct net_mgmt_event_callback ip_mgmt_cb;
static struct net_if *net_iface;

static bool wifi_connected;
static bool net_connected;

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface);
static void handle_wifi_connect();
static void handle_wifi_disconnect();
static void handle_net_available();
void wifi_conn_set_net_state_cb(void (*cb)(uint8_t wifi_state, uint8_t net_state));

static void (*net_state_cb)(uint8_t wifi_state, uint8_t net_state);

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface)
{
    LOG_INF("net mgmt handler, evt:%d", mgmt_event);
    switch (mgmt_event)
    {
    case NET_EVENT_IF_UP:
        break;
    case NET_EVENT_IF_DOWN:
        break;
    case NET_EVENT_ETHERNET_CARRIER_ON:
        handle_wifi_connect();
        break;
    case NET_EVENT_ETHERNET_CARRIER_OFF:
        handle_wifi_disconnect();
        break;
    case NET_EVENT_IPV4_DHCP_BOUND:
        handle_net_available();
    default:
        break;
    }
}

static void handle_wifi_connect()
{
    LOG_INF("wifi connect evt");
    net_state_cb(WIFI_CONN_STATE_UP, WIFI_CONN_STATE_NO_CHANGE);
}

static void handle_wifi_disconnect()
{
    LOG_INF("wifi disconnect evt");
    net_state_cb(WIFI_CONN_STATE_DOWN, WIFI_CONN_STATE_DOWN);
}

static void handle_net_available()
{
    LOG_INF("wifi net ip bound evt");
    net_connected = true;
    while (net_if_ipv4_get_global_addr(net_iface,NET_ADDR_ANY_STATE) == NULL)
    {
        printk("dont have ip,waiting\n");
        k_sleep(K_MSEC(1000));
    }
    net_state_cb(WIFI_CONN_STATE_NO_CHANGE, WIFI_CONN_STATE_UP);
}

int wifi_conn_init()
{
    wifi_connected = false;
    net_connected = false;

    net_mgmt_init_event_callback(&if_mgmt_cb, net_mgmt_event_handler,
                                IF_MGMT_EVENTS);
    net_mgmt_init_event_callback(&eth_mgmt_cb, net_mgmt_event_handler,
                                ETH_MGMT_EVENTS);
    net_mgmt_init_event_callback(&ip_mgmt_cb, net_mgmt_event_handler,
                                IP_MGMT_EVENTS);
    net_mgmt_add_event_callback(&if_mgmt_cb);
    net_mgmt_add_event_callback(&eth_mgmt_cb);
    net_mgmt_add_event_callback(&ip_mgmt_cb);

    net_iface = net_if_get_default();
    if (!net_iface) {
        LOG_ERR("counld not get netif");
        return -1;
    }

    net_dhcpv4_start(net_iface);

    return 0;
}

void wifi_conn_disconnect(struct k_work *work)
{
    int ret = 0;
    ret = esp_wifi_disconnect();

}

void wifi_conn_connect(struct k_work *work)
{
    esp_err_t ret;

    ret = esp_wifi_connect();
    if (ret != 0)
    {
        LOG_ERR("Failed to connect");
    }
}

void wifi_conn_reset(struct k_work *work)
{
    esp_err_t ret;
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != 0)
    {
        LOG_ERR("Failed to set wifi mode");
    }

}

void wifi_conn_setup(struct k_work *work)
{
    esp_err_t ret;
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (ret != 0)
    {
        LOG_ERR("Failed to configure wifi");
    }

}

void wifi_conn_set_net_state_cb(void (*cb)(uint8_t wifi_state, uint8_t net_state))
{
    net_state_cb = cb;
}