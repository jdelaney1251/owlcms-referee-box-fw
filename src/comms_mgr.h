#ifndef COMMS_MGR_H_
#define COMMS_MGR_H_

#define COMMS_MGR_DEC_RED       0
#define COMMS_MGR_DEC_BLK       1

int comms_mgr_init();

int comms_mgr_enter_wifi_sc_mode();
int comms_mgr_set_conn_evt_cb();
int comms_mgr_is_connected();
int comms_mgr_connect();
int comms_mgr_disconnect();

int comms_mgr_notify_decision(uint8_t decision);

#endif