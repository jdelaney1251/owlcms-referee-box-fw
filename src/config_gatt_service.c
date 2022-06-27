#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(config_gatt_service, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>  
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>

#include "config_gatt_service.h"
#include "settings_util.h"

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

static struct bt_uuid_128 mqtt_srv_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe43, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));

static struct bt_uuid_128 mqtt_port_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe44, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));

static struct bt_uuid_128 mqtt_client_name_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe45, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));

static struct bt_uuid_128 owlcms_platform_name_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe46, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));

static struct bt_uuid_128 config_write_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x0000fe47, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19));

static struct config_settings config;

static uint8_t wifi_ssid[32] = {};
static uint8_t wifi_psk[32] = {};
static uint8_t mqtt_srv[32] = {};
static uint16_t mqtt_port = 0;
static uint8_t mqtt_client_name[32] = {};
static uint8_t owlcms_platform_name[32] = {};

static ssize_t write_uint16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    uint8_t *value = attr->user_data;

    if (offset >= sizeof(uint16_t))
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    if (offset + len > sizeof(uint16_t))
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        
    memcpy(value + offset, buf, len);

    return len;
}

static ssize_t read_uint16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            void *buf, uint16_t len, uint16_t offset)
{
    const uint8_t *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(uint16_t));
}

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

static ssize_t config_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
    settings_util_set_wifi_ssid(wifi_ssid, strlen(wifi_ssid));
    settings_util_set_wifi_psk(wifi_psk, strlen(wifi_psk));

    struct mqtt_config_settings mqtt_config;
    strcpy(mqtt_config.broker_addr, mqtt_srv);
    mqtt_config.port = mqtt_port;
    strcpy(mqtt_config.client_name, mqtt_client_name);
    settings_util_set_mqtt_config(&mqtt_config);

    struct owlcms_config_settings owlcms_config;
    strcpy(owlcms_config.platform, owlcms_platform_name);
    settings_util_set_owlcms_config(&owlcms_config);

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
            read_value_wifi_psk, write_value_wifi_psk, &wifi_psk),
        BT_GATT_CHARACTERISTIC(&mqtt_srv_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_value_wifi_psk, write_value_wifi_psk, &mqtt_srv),
        BT_GATT_CHARACTERISTIC(&mqtt_port_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_uint16, write_uint16, &mqtt_port),
        BT_GATT_CHARACTERISTIC(&mqtt_client_name_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_value_wifi_psk, write_value_wifi_psk, &mqtt_client_name),
        BT_GATT_CHARACTERISTIC(&owlcms_platform_name_uuid.uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
            read_value_wifi_psk, write_value_wifi_psk, &owlcms_platform_name),
        BT_GATT_CHARACTERISTIC(&config_write_uuid.uuid,
            BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE,
            NULL, config_write, (void *)1)
);

//static struct bt_gatt_service config_service = BT_GATT_SERVICE(config_service_attrs);

//static void (* service_write_cb)(struct config_settings);

void config_gatt_service_init(struct config_settings *settings)
{
    int ret = 0;
    strcpy(wifi_ssid,   settings->wifi->ssid);
    strcpy(wifi_psk, settings->wifi->psk);
    strcpy(mqtt_srv, settings->mqtt->broker_addr);
    mqtt_port = settings->mqtt->port;
    strcpy(mqtt_client_name, "Empty");
    strcpy(owlcms_platform_name, settings->owlcms->platform);
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

