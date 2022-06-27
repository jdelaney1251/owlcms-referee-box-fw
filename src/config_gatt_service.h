#ifndef CONFIG_GATT_SERVICE_H_
#define CONFIG_GATT_SERVICE_H_

#include <stdint.h>
#include "settings_util.h"

#define SERVICE_UUID_CONFIG                 BT_UUID_DECLARE_16(0xa0001)

#define CHAR_UUID_VAL_WIFI_SSID             BT_UUID_DECLARE_16(0xb0001)
#define CHAR_UUID_VAL_WIFI_PSK              BT_UUID_DECLARE_16(0xb0002)

struct config_settings {
    struct wifi_config_settings *wifi;
    struct mqtt_config_settings *mqtt;
    struct owlcms_config_settings *owlcms;
};

void config_gatt_service_init(struct config_settings *config);
void config_gatt_service_remove();

void config_gatt_service_set_write_cb(void (*cb)(struct config_settings));

#endif