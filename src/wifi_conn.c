#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_mod, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ethernet_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/net_event.h>

#include "wifi_conn.h"
#include "settings_util.h"

#define IF_MGMT_EVENTS (NET_EVENT_IF_UP                    | \
                        NET_EVENT_IF_DOWN)

#define ETH_MGMT_EVENTS (NET_EVENT_ETHERNET_CARRIER_ON      | \
                         NET_EVENT_ETHERNET_CARRIER_OFF)

#define IP_MGMT_EVENTS (NET_EVENT_IPV4_DHCP_BOUND)

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT     | \
                          NET_EVENT_WIFI_DISCONNECT_RESULT  | \
                          NET_EVENT_WIFI_IFACE_STATUS)

static struct net_mgmt_event_callback if_mgmt_cb;
static struct net_mgmt_event_callback eth_mgmt_cb;
static struct net_mgmt_event_callback ip_mgmt_cb;
static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_if *net_iface;

static struct wifi_config_settings *wifi_config_params;

static bool wifi_connected;
static bool net_connected;

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface);
static void handle_wifi_connect(struct net_mgmt_event_callback *cb);
static void handle_wifi_disconnect(struct net_mgmt_event_callback *cb);
static void handle_net_available();
void wifi_conn_set_net_state_cb(void (*cb)(uint8_t wifi_state, uint8_t net_state));

static void (*net_state_cb)(uint8_t wifi_state, uint8_t net_state);

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface)
{
    LOG_INF("net mgmt handler, evt:%d %d", mgmt_event, NET_EVENT_ETHERNET_CARRIER_ON);
    switch (mgmt_event)
    {
    // case NET_EVENT_IF_UP:
    //     LOG_INF("if up evt");
    //     handle_wifi_connect();
    //     break;
    // case NET_EVENT_IF_DOWN:
    //     LOG_INF("if dn evt");
    //     handle_wifi_disconnect();
    //     break;
    // case NET_EVENT_ETHERNET_CARRIER_ON:
    //     handle_wifi_connect();
    //     break;
    // case NET_EVENT_ETHERNET_CARRIER_OFF:
    //     handle_wifi_disconnect();
    //     break;
    case NET_EVENT_IPV4_DHCP_BOUND:
        handle_net_available();

    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect(cb);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        handle_wifi_disconnect(cb);
        break;
    default:
        break;
    }
}

static void handle_wifi_connect(struct net_mgmt_event_callback *cb)
{
    
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    LOG_INF("wifi connect evt, status: %d", status->status);
    if (status->status == 0)
    {
        net_state_cb(WIFI_CONN_STATE_UP, WIFI_CONN_STATE_NO_CHANGE);
    }
}

static void handle_wifi_disconnect(struct net_mgmt_event_callback *cb)
{
    LOG_INF("wifi disconnect evt");
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    if (status->status == 0)
    {
        net_state_cb(WIFI_CONN_STATE_DOWN, WIFI_CONN_STATE_DOWN);
    }
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

    // net_mgmt_init_event_callback(&if_mgmt_cb, net_mgmt_event_handler,
    //                             IF_MGMT_EVENTS);
    // net_mgmt_init_event_callback(&eth_mgmt_cb, net_mgmt_event_handler,
    //                             ETH_MGMT_EVENTS);
    net_mgmt_init_event_callback(&ip_mgmt_cb, net_mgmt_event_handler,
                                IP_MGMT_EVENTS);
    // net_mgmt_add_event_callback(&if_mgmt_cb);
    // net_mgmt_add_event_callback(&eth_mgmt_cb);
    net_mgmt_add_event_callback(&ip_mgmt_cb);

    net_mgmt_init_event_callback(&wifi_mgmt_cb, net_mgmt_event_handler,
                                 WIFI_MGMT_EVENTS);

    net_mgmt_add_event_callback(&wifi_mgmt_cb);

    net_iface = net_if_get_default();
    if (!net_iface) {
        LOG_ERR("counld not get wifi iface");
        return -1;
    }

    net_dhcpv4_start(net_iface);

    return 0;
}

void wifi_conn_disconnect(struct k_work *work)
{
    int ret = 0;
    // ret = esp_wifi_disconnect();
    ret =  net_mgmt(NET_REQUEST_WIFI_DISCONNECT, net_iface, NULL, 0);
}

void wifi_conn_connect(struct k_work *work)
{
    uint32_t ret;

    if (!net_if_is_up(net_iface))
    {
        while(!net_if_is_up(net_iface))
        {
            k_sleep(K_MSEC(1000));
            LOG_DBG("waiting for wifi iface to come up");
        }
    }

    ret = esp_wifi_connect();
    struct wifi_connect_req_params wifi_params;
    wifi_params.ssid = wifi_config_params->ssid;
    wifi_params.ssid_length = wifi_config_params->ssid_length;
    wifi_params.psk = wifi_config_params->psk;
    wifi_params.psk_length = wifi_config_params->psk_length;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;

    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, net_iface, &wifi_params, sizeof(wifi_params));
    if (ret != 0)
    {
        LOG_ERR("Failed to connect %d", ret);
    }
}

void wifi_conn_reset(struct k_work *work)
{
    // esp_err_t ret;
    // ret = esp_wifi_set_mode(ESP32_WIFI_MODE_STA);
    // if (ret != 0)
    // {
    //     LOG_ERR("Failed to set wifi mode: %d", ret);
    // }

}

void wifi_conn_setup(struct wifi_config_settings *params)
{
    // esp_err_t ret;
    // wifi_config_t cfg = {
    //     .sta = {
    //         .ssid = " ",
    //         .password = " "
    //     }
    // };
    // strcpy(cfg.sta.ssid, params->ssid);
    // strcpy(cfg.sta.password, params->psk);
    // ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg);

    wifi_config_params = params;
    // if (ret != 0)
    // {
    //     LOG_ERR("Failed to configure wifi: %d", ret);
    // }

}

void wifi_conn_set_net_state_cb(void (*cb)(uint8_t wifi_state, uint8_t net_state))
{
    net_state_cb = cb;
}