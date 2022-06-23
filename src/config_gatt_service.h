#ifndef CONFIG_GATT_SERVICE_H_
#define CONFIG_GATT_SERVICE_H_

#include <stdint.h>

#define SERVICE_UUID_CONFIG                 BT_UUID_DECLARE_16(0xa0001)

#define CHAR_UUID_VAL_WIFI_SSID             BT_UUID_DECLARE_16(0xb0001)
#define CHAR_UUID_VAL_WIFI_PSK              BT_UUID_DECLARE_16(0xb0002)

struct config_settings {
    uint8_t *wifi_ssid;
    uint8_t wifi_ssid_len;
    uint8_t *wifi_psk;
    uint8_t wifi_psk_len;
};

void config_gatt_service_init(struct config_settings config);
void config_gatt_service_remove();

void config_gatt_service_set_write_cb(void (*cb)(struct config_settings));

#endif