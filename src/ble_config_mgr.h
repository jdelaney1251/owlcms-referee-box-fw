#ifndef BLE_CONFIG_MGR_H_
#define BLE_CONFIG_MGR_H_

#include "config_gatt_service.h"
// signal to the config mgr thread to enter config state
int ble_config_mgr_start(struct config_settings *settings);

// signal to the config mgr thread to leave config state
int ble_config_mgr_stop();

#endif