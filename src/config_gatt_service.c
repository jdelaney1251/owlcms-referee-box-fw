#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(config_gatt_service, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>  
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>

#include "config_gatt_service.h"

#define READ_VAL_CB_ARGS         struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset
#define WRITE_VAL_CB_ARGS       struct bt_conn *conn, \
			      const struct bt_gatt_attr *attr, const void *buf,\
			      uint16_t len, uint16_t offset, uint8_t flags


static struct bt_uuid_128 config_service_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe40, 0xcc7a, 0x482a, 0x984a, 0x7f2ed5b3e58f));

static struct bt_uuid_128 wifi_ssid_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe41, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));


static struct bt_uuid_128 wifi_psk_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe42, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));



static struct config_settings config;

static uint8_t wifi_ssid[32] = {};
static uint8_t wifi_psk[32] = {};


static ssize_t read_value_wifi_ssid(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
    //LOG_INF("read wifi ssid");
    const char *value = attr->user_data;
    //LOG_INF("%d, %d, %d", conn, attr, buff)
    //LOG_INF("val %s", value);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t write_value_wifi_ssid(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
    if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
		return 0;
	}

	if (offset + len > 32) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
    //LOG_INF("write wifi ssid");
    uint8_t *value = attr->user_data;
    memcpy(value + offset, buf, len);
    value[offset + len] = 0;
    return len;
}

static ssize_t read_value_wifi_psk(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
    //LOG_INF("read wifi psk");
    const char *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t write_value_wifi_psk(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
    if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
		return 0;
	}

	if (offset + len > 32) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
    //LOG_INF("write wifi psk");
    uint8_t *value = attr->user_data;
    memcpy(value + offset, buf, len);
    value[offset + len] = 0;
    return len;
}


// struct bt_gatt_attr config_service_attrs[] = {
//     BT_GATT_PRIMARY_SERVICE(&config_service_uuid),
//     BT_GATT_CHARACTERISTIC(&wifi_ssid_uuid.uuid,
//         BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
//         BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
//         read_value_wifi_ssid, write_value_wifi_ssid, &config.wifi_ssid),
//     BT_GATT_CHARACTERISTIC(&wifi_psk_uuid.uuid,
//         BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
//         BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
//         read_value_wifi_psk, write_value_wifi_psk, &config.wifi_psk)
// };

BT_GATT_SERVICE_DEFINE(config_service,
    BT_GATT_PRIMARY_SERVICE(&config_service_uuid),
        BT_GATT_CHARACTERISTIC(&wifi_ssid_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_value_wifi_ssid, write_value_wifi_ssid, &wifi_ssid),
        BT_GATT_CHARACTERISTIC(&wifi_psk_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_value_wifi_psk, write_value_wifi_psk, &wifi_psk)
);

//static struct bt_gatt_service config_service = BT_GATT_SERVICE(config_service_attrs);

//static void (* service_write_cb)(struct config_settings);

void config_gatt_service_init(struct config_settings cfg)
{
    int ret = 0;
    strcpy(wifi_ssid, "Empty");
    strcpy(wifi_psk, "Empty");
    //snprintk(config.wifi_ssid, 10, "this is a");
    //memcpy(config.wifi_ssid, "test str  ", 10);

    // ret = bt_gatt_service_register(&config_service);
    // if (ret == 0)
    // {
    //     LOG_INF("registered service");
    // }
    // else {
    //     LOG_ERR("failed to register service %d", ret);
    // }

}

void config_gatt_service_remove()
{
    //bt_gatt_service_unregister(&config_service);
}

void config_gatt_service_set_write_cb(void (*cb)(struct config_settings))
{
    //service_write_cb = cb;
}

