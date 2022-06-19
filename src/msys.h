#ifndef SMSYS_H_
#define SMSYS_H_

#define SYS_EVT_CONN_SUCCESS            2
#define SYS_EVT_CONN_LOST               3
#define SYS_EVT_INP_RED_DECISION        4
#define SYS_EVT_INP_BLK_DECISION        5
#define SYS_EVT_DECISION_HANDLED        6
#define SYS_EVT_DECISION_SEND_ERR       7
#define SYS_EVT_DECISION_REQ_RX         8

int msys_init();
int msys_run();

int msys_signal_evt(uint8_t evt);


#endif