#ifndef UART_CONFIG_MGR_H_
#define UART_CONFIG_MGR_H_

int uart_config_mgr_init();
// // signal to the uart config mgr to enter the config state and start
// // accepting config commands over uart
// int uart_config_mgr_start(struct config_settings *settings);

// // signal to the config mgr to exit the config state and return the 
// // uart to its previous state 
// int uart_config_mgr_stop();

#endif