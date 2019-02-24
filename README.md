# MQTT Alarm Keypad

A keypad to arm/disarm alarm based on the platform Wemos (ESP8266).

The keypad uses wemos pins:

```
 keypad1 -- D0
 keypad2 -- D1
 keypad3 -- D2
 keypad4 -- D3
 keypad5 -- D4
 keypad6 -- D5
 keypad7 -- D6
```

3 LEDs WS2812B the data pin, to indicate the current state, uses the pin D7

The device communicates with Home Assistant with MQTT.

# MQTT Topics

`alarm/keypad` - the device publishes its state on this topic. A JSON document is sent every 10 minuts
```
  {
    "ip": "192.168.1.102",
    "mac": "88:FF:EE:44:EE:00",
    "rssi": "-57",
    "uptime": "0T14:10:03.543"
  }
```

`alarm/keypad/status` online|offline - status of the device

`alarm/keypad/command` - the topic which the device is subscribed to. It takes commands to perform an action.
Currently there is only one command supported

```
  {
    "command": "lock",
    "duration": 20
  }
```
It locks the keypad for `duration` seconds.

