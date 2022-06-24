#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_config_mgr, LOG_LEVEL_DBG);


#include <zephyr/zephyr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble_config_mgr.h"
#include "config_gatt_service.h"

#define BLE_CONFIG_MGR_THREAD_STACK_SIZE        4096

#define DEVICE_NAME             "BLE Config Device12"
#define DEVICE_NAME_LEN         20

struct bt_conn *conn;

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void ble_host_connected(struct bt_conn *connected, uint8_t err);
static void ble_host_disconnected(struct bt_conn *connected, uint8_t reason);

struct config_settings settings_temp;

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = ble_host_connected,
    .disconnected = ble_host_disconnected,
};

int ble_config_mgr_init()
{
    int ret = 0;

    ret = bt_enable(NULL);
    if (ret != 0)
    {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }


    //k_thread_start(ble_config_mgr_th);
    
    return ret;
}

int ble_config_mgr_deinit()
{
    int ret = 0;

    ret = bt_disable();
    if (ret != 0)
    {
        LOG_ERR("Bluetooth deinit failed: %d", ret);
        return ret;
    }
    return ret;
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

int ble_config_mgr_start()
{
    int ret = 0;
    ble_config_mgr_init();
    config_gatt_service_init(settings_temp);

    ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    //bt_conn_auth_cb_register(&auth_cb_display);

    return ret;
}

int ble_config_mgr_stop()
{
    ble_config_mgr_deinit();
}

static void ble_host_connected(struct bt_conn *connected, uint8_t err)
{
    if (err != 0)
        LOG_INF("connection error: 0x%02x", err);
    else
    {
        LOG_INF("host connected");
        if (!conn)
        {
            conn = bt_conn_ref(connected);
        }
    }
}

static void ble_host_disconnected(struct bt_conn *connected, uint8_t reason)
{
    LOG_INF("disconnected, reason: 0x%02x", reason);
    if (conn)
    {
        bt_conn_unref(connected);
        conn = NULL;
    }
}

