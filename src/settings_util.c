#include <logging/log.h>

LOG_MODULE_REGISTER(settings_util, LOG_LEVEL_ERR);

#include <string.h>

#include <zephyr.h>
#include <settings/settings.h>
#include <zephyr/settings/settings.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <net/wifi_mgmt.h>

#include "settings_util.h"


#define SETTING_TYPE_UINT8              0
#define SETTING_TYPE_STR                1
#define SETTING_TYPE_UINT16             2

#define SETTING_TYPE_STR_MAXLEN         32

typedef struct setting_entry {
    uint8_t id;
    uint8_t type;
    char *name;
    void *data;
    uint8_t data_len;
} setting_entry_t;

enum setting_ids {
    SSID_ID,
    SSID_LEN_ID,
    PSK_ID,
    PSK_LEN_ID,
    MQTT_SRV_ID,
    MQTT_SRV_LEN_ID,
    MQTT_PORT_ID,
    MQTT_CLIENT_NAME_ID,
    MQTT_CLIENT_NAME_LEN_ID,
    OWLCMS_PLATFORM_NAME_ID,
    OWLCMS_PLATFORM_NAME_LEN_ID,
    NUM_SETTINGS
};



static setting_entry_t settings[] = {
    {   .id = SSID_ID, 
        .type = SETTING_TYPE_STR, 
        .name = "ssid",
        .data = NULL,
        .data_len = SETTING_TYPE_STR_MAXLEN * sizeof(uint8_t)
    },
    {   .id = SSID_LEN_ID, 
        .type = SETTING_TYPE_UINT8,
        .name = "ssid_length", 
        .data = NULL, 
        .data_len = sizeof(uint8_t)
    },
    {   .id = PSK_ID, 
        .type = SETTING_TYPE_STR, 
        .name = "psk",
        .data = NULL,
        .data_len = SETTING_TYPE_STR_MAXLEN * sizeof(uint8_t)
    },
    {   .id = PSK_LEN_ID, 
        .type = SETTING_TYPE_UINT8, 
        .name = "psk_length",
        .data = NULL,
        .data_len = sizeof(uint8_t)
    },
    {
        .id = MQTT_SRV_ID,
        .type = SETTING_TYPE_STR,
        .name = "mqtt_srv",
        .data = NULL,
        .data_len = SETTING_TYPE_STR_MAXLEN * sizeof(uint8_t)
    },
    {   .id = MQTT_SRV_LEN_ID, 
        .type = SETTING_TYPE_UINT8,
        .name = "mqtt_srv_length", 
        .data = NULL, 
        .data_len = sizeof(uint8_t)
    },
    {   .id = MQTT_PORT_ID, 
        .type = SETTING_TYPE_UINT16,
        .name = "mqtt_port", 
        .data = NULL, 
        .data_len = sizeof(uint16_t)
    },
    {
        .id = MQTT_CLIENT_NAME_ID,
        .type = SETTING_TYPE_STR,
        .name = "mqtt_client_name",
        .data = NULL,
        .data_len = SETTING_TYPE_STR_MAXLEN * sizeof(uint8_t)
    },
    {   .id = MQTT_CLIENT_NAME_LEN_ID, 
        .type = SETTING_TYPE_UINT8,
        .name = "mqtt_client_name_length", 
        .data = NULL, 
        .data_len = sizeof(uint8_t)
    },
    {   .id = OWLCMS_PLATFORM_NAME_ID, 
        .type = SETTING_TYPE_STR, 
        .name = "platform",
        .data = NULL,
        .data_len = SETTING_TYPE_STR_MAXLEN * sizeof(uint8_t)
    },
    {   .id = OWLCMS_PLATFORM_NAME_LEN_ID, 
        .type = SETTING_TYPE_UINT8,
        .name = "platform", 
        .data = NULL, 
        .data_len = sizeof(uint8_t)
    },
};


static struct nvs_fs fs;
#define STORAGE_NODE_LABEL  storage

/*
static struct wifi_config_settings wifi_config = {
    .ssid_length = 4,
    .ssid = "abcde",
    .psk_length = 4,
    .psk = "abcde"
};

int wifi_settings_handle_set(const char *name, size_t len,
                             settings_read_cb read_cb, 
                             void *cb_arg);
int wifi_settings_handle_commit(void);
int wifi_settings_handle_export(int (*cb)(const char *name, 
                             const void *value,
                             size_t val_len));

struct settings_handler wifi_settings_handler = {
    .name = "wifi_config",
    .h_get = wifi_settings_handle_set,
    .h_set = wifi_settings_handle_set
};

SETTINGS_STATIC_HANDLER_DEFINE(wifi_config, "wifi_config", NULL, wifi_settings_handle_set, NULL,NULL);

int wifi_settings_handle_set(const char *name, size_t len,
                             settings_read_cb read_cb, 
                             void *cb_arg)
{
    const char *next;
    size_t next_len;
    int ret;

    LOG_INF("handle set %s", name);
    next_len = settings_name_next(name, &next);
    if (!next)
    {
        return -ENOENT;
    }

    if (!strncmp(name, "ssid_length", next_len))
    {
        if (!next)
        {
            ret = read_cb(cb_arg, &wifi_config.ssid_length, sizeof(wifi_config.ssid_length));
            return 0;
        }
        return -ENOENT;
    }

    if (!strncmp(name, "ssid", next_len))
    {
        if (!next)
        {
            wifi_config.ssid = k_malloc(wifi_config.ssid_length * sizeof(char));
            ret = read_cb(cb_arg, &wifi_config.ssid, wifi_config.ssid_length*sizeof(char));
        }
        return -ENOENT;
    }
    return -ENOENT;
}

int settings_util_init()
{
    int ret = 0;
    ret = settings_subsys_init();
    //settings_register(&wifi_settings_handler);
    LOG_INF("settings util init ret %d", ret);
}

int settings_util_load_wifi_config(struct wifi_connect_req_params *params)
{
    int ret = 0;
    ret = settings_load();
    LOG_INF("settings load ret %d", ret);
    params->ssid_length = wifi_config.ssid_length;
    params->psk_length = wifi_config.psk_length;
    params->ssid = k_malloc(params->ssid_length*sizeof(uint8_t));
    params->psk = k_malloc(params->psk_length*sizeof(uint8_t));
}

int settings_util_set_wifi_ssid(const char *ssid, uint8_t len)
{
    int ret = 0;
    ret = settings_save_one("wifi_config/ssid_length", (const void *)&len, sizeof(len));
    LOG_INF("settings save one ssid len: %d, ret %d", len, ret);
}
*/

void load_or_create_settings();
void write_empty_setting(setting_entry_t *entry);
void save_setting(uint8_t setting_id);

void write_empty_setting(setting_entry_t *entry)
{
    int ret = 0;

    // if (entry->type == SETTING_TYPE_UINT8)
    // {
    //     entry->data = (uint8_t)0;
    // }
    // else if (entry->type == SETTING_TYPE_STR)
    // {
    //     for (uint8_t i = 0; i < entry->data_len; i++)
    //     {
    //         memset(entry->data, 0, entry->data_len);
    //     }
    // }
    // else if (entry->type == SETTING_TYPE_UINT16)
    // {
    //     entry->data = (uint16_t)0;
    // }

    memset(entry->data, 0, entry->data_len);

    nvs_write(&fs, entry->id, entry->data, entry->data_len);
}

void save_setting(uint8_t setting_id)
{
    if (setting_id < NUM_SETTINGS)
    {
        nvs_write(&fs, settings[setting_id].id, settings[setting_id].data,
                        settings[setting_id].data_len);
    }
}

void load_or_create_settings()
{
    int ret = 0;

    LOG_DBG("check for settings");
    for (uint8_t i = 0; i < NUM_SETTINGS; i++)
    {
        if (settings[i].data == NULL)
        {
            settings[i].data = k_malloc(settings[i].data_len);
        }
        ret = nvs_read(&fs, settings[i].id, settings[i].data, settings[i].data_len);
        if (ret <= 0)
        {
            LOG_DBG("creating new setting entry for %s, ret %d", settings[i].name, ret); 
            write_empty_setting(&settings[i]);
        }
    }
}

int settings_util_init()
{
    int ret = 0;
    struct flash_pages_info info;

    LOG_DBG("init");
    fs.flash_device = FLASH_AREA_DEVICE(STORAGE_NODE_LABEL);
    if (!device_is_ready(fs.flash_device))
    {
        LOG_ERR("Flash device not ready");
        return -EIO;
    }

    fs.offset = FLASH_AREA_OFFSET(STORAGE_NODE_LABEL);
    ret = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (ret)
    {
        LOG_ERR("Unable to get flash page info");
        return -EIO;
    }

    fs.sector_count = 3U;
    fs.sector_size = info.size;
    LOG_DBG("flash offset %x", fs.offset);

    ret = nvs_mount(&fs);
    if (ret)
    {
        LOG_ERR("Flash init failed %d", ret);
        return -EIO;
    }
    
    load_or_create_settings();
}

int settings_util_load_wifi_config(struct wifi_config_settings *params)
{
    //LOG_DBG("loading wifi params from settings");
    //memcpy(params->ssid_length, settings[SSID_LEN_ID].data, settings[SSID_LEN_ID].data_len);
    //LOG_DBG("here");
    //memcpy(params->psk_length, settings[PSK_LEN_ID].data, settings[PSK_LEN_ID].data_len);
    if (params->ssid == NULL)
    {
        params->ssid = k_malloc(sizeof(uint8_t) * SETTING_TYPE_STR_MAXLEN);
    }

    if (params->psk == NULL)
    {
        params->psk = k_malloc(sizeof(uint8_t) * SETTING_TYPE_STR_MAXLEN);
    }

    //memcpy(params->ssid, settings[SSID_ID].data, params->ssid_length);
    //memcpy(params->psk, settings[PSK_ID].data, params->psk_length);

    strcpy(params->ssid, settings[SSID_ID].data);
    strcpy(params->psk, settings[PSK_ID].data);
}

int settings_util_set_wifi_ssid(const char *ssid, uint8_t len)
{
    //LOG_DBG("copy ssid");
    //memcpy(settings[SSID_ID].data, ssid, len);
    if (ssid != NULL)
        strcpy(settings[SSID_ID].data, ssid);
    //LOG_DBG("copy ssid_len: data: %d, len: %d", len, settings[SSID_LEN_ID].data_len);
    //*settings[SSID_LEN_ID].data = len;
    //memcpy(settings[SSID_LEN_ID].data, &len, settings[SSID_LEN_ID].data_len);
    //printk("%d", (uint8_t *)settings[SSID_LEN_ID].data);
    //LOG_DBG("save ssid");
    save_setting(SSID_ID);
    //LOG_DBG("save ssid_len");
    save_setting(SSID_LEN_ID);
}

int settings_util_set_wifi_psk(const char *psk, uint8_t len)
{
    //memcpy(settings[PSK_ID].data, psk, len);
    if (psk != NULL)
        strcpy(settings[PSK_ID].data, psk);
    //*settings[PSK_LEN_ID].data = len;
    //memcpy(settings[PSK_LEN_ID].data, &len, settings[PSK_LEN_ID].data_len);
    save_setting(PSK_ID);
    save_setting(PSK_LEN_ID);
}

int settings_util_load_mqtt_config(struct mqtt_config_settings *params)
{
    if (params->broker_addr != NULL)
        strcpy(params->broker_addr, settings[MQTT_SRV_ID].data);
    memcpy(&(params->port), settings[MQTT_PORT_ID].data, sizeof(params->port));
    
    return 0;
}

int settings_util_set_mqtt_config(struct mqtt_config_settings *params)
{
    /*
    params->broker_addr = "10.0.0.7";
    params->broker_addr_len = 8;
    params->port = 1883;
    params->client_name = "owlcms_ref_1";
    params->client_name_len = 12;
    */
    strcpy(settings[MQTT_SRV_ID].data, params->broker_addr);
    //settings[MQTT_PORT_ID].data = (uint16_t)params->port;
    memcpy(settings[MQTT_PORT_ID].data, &(params->port), settings[MQTT_PORT_ID].data_len);

    save_setting(MQTT_SRV_ID);
    save_setting(MQTT_PORT_ID);
    return 0;
}

int settings_util_load_owlcms_config(struct owlcms_config_settings *params)
{
    if (params->platform != NULL)
        strcpy(params->platform, settings[OWLCMS_PLATFORM_NAME_ID].data);
    
    return 0;
}

int settings_util_set_owlcms_config(struct owlcms_config_settings *params)
{
    /*
    if (params->platform == NULL)
        params->platform = k_malloc(sizeof(uint8_t));
    strcpy(params->platform, "A");
    params->platform_len = 1;
    */
   strcpy(settings[OWLCMS_PLATFORM_NAME_ID].data, params->platform);

   save_setting(OWLCMS_PLATFORM_NAME_ID);

   return 0;
}