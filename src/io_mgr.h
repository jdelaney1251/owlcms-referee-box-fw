#ifndef IO_MGR_
#define IO_MGR_

int io_mgr_init();

int io_mgr_set_leds_connecting();
int io_mgr_set_leds_bat_level();
int io_mgr_set_leds_disable();

int io_mgr_set_leds_config();

int io_mgr_buzzer_trig();


#endif