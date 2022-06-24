#ifndef COMMS_MGR_H_
#define COMMS_MGR_H_

#define COMMS_MGR_DEC_BLK       0
#define COMMS_MGR_DEC_RED       1

int comms_mgr_init();

int comms_mgr_is_connected();
int comms_mgr_connect();
int comms_mgr_disconnect();

int comms_mgr_start_config();
int comms_mgr_end_config();

int comms_mgr_notify_decision(uint8_t decision);

#endif