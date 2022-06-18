#ifndef _MQTT_CLIENT_H_
#define _MQTT_CLIENT_H_

int mqtt_client_mod_init();
int mqtt_client_publish();
int mqtt_client_subscribe(const char *topic, void (*handler)(uint8_t *topic, uint8_t *msg));
int mqtt_client_setup();
int mqtt_client_start();

int mqtt_client_teardown();

#endif