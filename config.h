#ifndef MQTT_ALARM_PANEL_CONFIG_H
#define MQTT_ALARM_PANEL_CONFIG_H

#define DIGITS 4

#define INTERVAL_PUBLISH_STATE 600000 // 10min

#define MQTT_TOPIC_STATE "alarm/keypad"
#define MQTT_TOPIC_CODE "alarm/keypad/code"
#define MQTT_TOPIC_COMMAND "alarm/keypad/command"

#define MQTT_TOPIC_STATUS "alarm/keypad/status"
#define MQTT_STATUS_PAYLOAD_ON "online"
#define MQTT_STATUS_PAYLOAD_OFF "offline"

#define WIFI_AP_NAME "AlarmKeypad"
#define WIFI_AP_PASS "123456"

#endif // MQTT_ALARM_PANEL_CONFIG_H
