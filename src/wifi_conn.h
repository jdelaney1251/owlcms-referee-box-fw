#ifndef WIFI_CONN_H_
#define WIFI_CONN_H_

#define WIFI_CONN_STATE_NO_CHANGE   0
#define WIFI_CONN_STATE_UP          1
#define WIFI_CONN_STATE_DOWN        2

#include "settings_util.h"

int wifi_conn_init();

void wifi_conn_connect();
void wifi_conn_disconnect();
void wifi_conn_reset();
void wifi_conn_setup(struct wifi_config_settings *params);

void wifi_conn_set_net_state_cb(void (*cb)(uint8_t wifi_state, uint8_t net_state));

#endif