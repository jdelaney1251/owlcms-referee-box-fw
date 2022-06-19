#ifndef _MQTT_CLIENT_H_
#define _MQTT_CLIENT_H_

#define MQTT_STATE_NO_CHANGE        0
#define MQTT_STATE_CONNECTED        1
#define MQTT_STATE_DISCONNECTED     2

int mqtt_client_mod_init();
int mqtt_client_publish(enum mqtt_qos qos, 
                            uint8_t *topic,
                            uint32_t topic_len,
                            uint8_t *data, 
                            uint32_t data_len);
int mqtt_client_subscribe(const char *topic, void (*handler)(uint8_t *topic, uint8_t *msg));
int mqtt_client_setup();
int mqtt_client_start();

int mqtt_client_teardown();

void mqtt_client_set_state_cb(void (*cb)(uint8_t mqtt_state));

#endif