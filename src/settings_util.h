#ifndef SETTINGS_UTIL_H_
#define SETTINGS_UTIL_H_

#include <net/wifi_mgmt.h>


struct wifi_config_settings {
    char *ssid;
    uint8_t ssid_length;
    char *psk;
    uint8_t psk_length;
};

struct mqtt_config_settings {
    char broker_addr[32];
    uint8_t broker_addr_len;
    uint16_t port;
    char client_name[32];
    uint8_t client_name_len;
};

struct owlcms_config_settings {
    char platform[32];
    uint8_t platform_len;
};

struct config_settings {
    struct wifi_config_settings *wifi;
    struct mqtt_config_settings *mqtt;
    struct owlcms_config_settings *owlcms;
};

int settings_util_init();

int settings_util_load_wifi_config(struct wifi_config_settings *params);
int settings_util_set_wifi_ssid(const char *ssid, uint8_t len);
int settings_util_set_wifi_psk(const char *psk, uint8_t len);

int settings_util_set_mqtt_config(struct mqtt_config_settings *params);
int settings_util_load_mqtt_config(struct mqtt_config_settings *params);
int settings_util_set_owlcms_config(struct owlcms_config_settings *params);
int settings_util_load_owlcms_config(struct owlcms_config_settings *params);

#endif