#include "MyQueueArray.h"
#include "config.h"

// https://escapequotes.net/esp8266-wemos-d1-mini-pins-and-diagram/
// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager

#include <AsyncMqttClient.h>      // https://github.com/marvinroger/async-mqtt-client - Async MQTT client

#include <Keypad.h>
#include <FS.h>

#include <SimpleTimer.h>          // https://github.com/schinken/SimpleTimer

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson

// timer
SimpleTimer timer;

// MQTT
char mqtt_server[36];
char mqtt_port[6] = "1883";
char mqtt_login[36];
char mqtt_password[36];

// MQTT client
AsyncMqttClient mqttClient;

// WiFi Manager
// Flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Keypad
const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
char keys[ROWS][COLS] =
{
  {'1','2','3' },
  {'4','5','6' },
  {'7','8','9' },
  {'*','0','#' }
};

//  keypad:  1   2   3   4   5   6   7
//  pins:    C2  R1  C1  R4  C3  R3  R2
//  ESP:     2   4   5   12  13  14  16
//  Wem:  D0  D1  D2 D3  D4  D5 D6

byte rowPins[ROWS] = {D1, D6, D5, D3};  //{R1, R2, R3, R4}
byte colPins[COLS] = {D2, D0, D4};       //{C1, C2, C3}

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
QueueArray<char> queueInputCode(DIGITS);

// Pixels
#define LED_PIN D7
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(DIGITS, LED_PIN, NEO_GRB + NEO_KHZ800);


// Wating animation globals
bool waActive = false;
unsigned int waCount = 0;
byte waActiveLED = 0;
bool waForward = true;

// Error animation globals
bool errActive = false;

// Lock the keypad
unsigned long lock_endtime = 0;


void readConfigurationFile() {
  //read configuration from FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_login, json["mqtt_login"]);
          strcpy(mqtt_password, json["mqtt_password"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      Serial.println("/config.json not found");
    }
  } else {
    Serial.println("failed to mount FS");
  }

  Serial.println(mqtt_server);
  Serial.println(mqtt_port);
  Serial.println(mqtt_login);
  Serial.println(mqtt_password);
}


void writeConfigurationFile() {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_login"] = mqtt_login;
  json["mqtt_password"] = mqtt_password;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  } else {
    Serial.println("Save data in config file");
  }

  json.prettyPrintTo(Serial);
  json.printTo(configFile);
  configFile.close();
  shouldSaveConfig = false;
}


void createCustomWiFiManager() {
  // The extra parameters to be configured
  WiFiManagerParameter custom_text("<p>MQTT Server</p>");
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT port", mqtt_port, 5);
  WiFiManagerParameter custom_mqtt_login("mqtt_login", "MQTT login", mqtt_login, 64);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT password", mqtt_password, 64);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all custom parameters
  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_login);
  wifiManager.addParameter(&custom_mqtt_password);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASS)) {
    Serial.println("Failed to connect and hit timeout");
  }

  //if you get here you have connected to the WiFi
  Serial.println("Connected to WiFi");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_login, custom_mqtt_login.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
}


// ---------------------------------------------------------
char *uptime(unsigned long milli)
{
  static char _return[32];
  unsigned long secs=milli/1000, mins=secs/60;
  unsigned int hours=mins/60, days=hours/24;
  milli-=secs*1000;
  secs-=mins*60;
  mins-=hours*60;
  hours-=days*24;
  sprintf(_return,"%dT%2.2d:%2.2d:%2.2d.%3.3d", (byte)days, (byte)hours, (byte)mins, (byte)secs, (int)milli);
  return _return;
}

// MQTT ---------------------------------------------
/* Send an input code as an mqtt message */
void sendCode() {
  if (!queueInputCode.isEmpty ()) {
    char buffer[DIGITS + 1];
    byte index = 0;
    while ( !queueInputCode.isEmpty() ) {
      buffer[index++] = queueInputCode.dequeue();
    }
    buffer[index] = 0;
    Serial.printf("Send code: %s\n", buffer);
    mqttClient.publish(MQTT_TOPIC_CODE, 1, false, buffer);
  } else {
    Serial.println("The code is empty. Nothing to send.");
  }
}

/* Publish the current state odf the device. It will be called perioudically. */
void publishState() {
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(20);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  char ip[16];
  memset(ip, 0, 18);
  sprintf(ip, "%s", WiFi.localIP().toString().c_str());
  root["ip"] = ip;

  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  char mac[18];
  memset(mac, 0, 18);
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  root["mac"] = mac;

  char rssi[8];
  sprintf(rssi, "%d", WiFi.RSSI());
  root["rssi"] = rssi;

  root["uptime"] = uptime( millis() );

  // Firmware version
  root["version"] = FIRMWARE_VERSION;

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  Serial.printf("\nMQTT: Publish state: %s\n", buffer);

  mqttClient.publish(MQTT_TOPIC_STATE, 0, true, buffer);
}


void onMqttConnect(bool sessionPresent) {
  Serial.println("MQTT: Connected");
  Serial.printf("MQTT: Session present: %d\n", sessionPresent);

  Serial.print("MQTT: Subscribing at QoS 0, topic: ");
  Serial.println(MQTT_TOPIC_COMMAND);
  mqttClient.subscribe(MQTT_TOPIC_COMMAND, 0);

  Serial.print("MQTT: Publish online status: ");
  Serial.println(MQTT_TOPIC_STATUS);
  mqttClient.publish(MQTT_TOPIC_STATUS, 1, true, MQTT_STATUS_PAYLOAD_ON);

  waActive = false;
  publishState();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  waActive = true;

  Serial.println();
  Serial.print("MQTT: Disconnected: ");
  if (reason == AsyncMqttClientDisconnectReason::TCP_DISCONNECTED) {
    Serial.println("TCP disconnected");
  } else if (reason == AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION) {
    Serial.println("Unacceptable protocol version");
  } else if (reason == AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED) {
    Serial.println("Indentifier rejected");
  } else if (reason == AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE) {
    Serial.println("Server unavailable");
  } else if (reason == AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS) {
    Serial.println("Malformed credentials");
  } else if (reason == AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED) {
    Serial.println("Not authorized");
  } else {
    Serial.println("Unknown reason");
  }

  delay(3000);
  Serial.println("MQTT: Reconnecting to broker...");
  if (WiFi.isConnected()) {
    mqttClient.connect();
  }
}


void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println();
  Serial.println("MQTT: Message received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);

  Serial.print("  payload: ");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(payload);
  json.printTo(Serial);
  Serial.println();

  if (json.success()) {
    if (json.containsKey("command")) {
      const char* command = json["command"];
      String stringCommand(command);
      if (stringCommand == String("lock")) {
        byte duration = 60;
        if (json.containsKey("duration")) {
          duration = (byte)atoi(json["duration"]);
        }
        Serial.printf("Lock keypad for %d seconds\n", duration);
        lock_endtime = millis() + duration * 1000;
        errActive = true;
      } else {
        Serial.printf("Unknown command: %s\n", command);
      }
    } else {
      Serial.printf("Payload doesn't contain any command\n");
    }
  } else {
    Serial.println("Wrong JSON document");
  }
}


// ----------------------------------------------
/*
 * The function shows a wating animation as a running blue light
 * from right to left and back.
 */
void waitingAnimation() {
  if (waActive) {
    unsigned int count = millis() / 100;
    if (waCount != count) {
      waCount = count;
      if (waForward) {
        waActiveLED += 1;
      } else {
        waActiveLED -= 1;
      }
      if (waActiveLED == 0) {
        waForward = true;
      } else if (waActiveLED == (DIGITS-1)) {
        waForward = false;
      }
      for (int c = 0; c < DIGITS; c++) {
        byte clr = waActiveLED == c ? 255 : 0;
        pixels.setPixelColor(c, pixels.Color(0,0,clr));
      }
    }
  }
}

/*
 * The function shows an error animation as a flashing red light.
 */
void errorAnimation() {
  if (errActive) {
    unsigned int count = millis() / 250;
    if (waCount != count) {
      waCount = count;
      if ( waCount %2 ) {
        pixels.setPixelColor(0, pixels.Color(255,0,0));
        pixels.setPixelColor(1, pixels.Color(255,0,0));
        pixels.setPixelColor(2, pixels.Color(255,0,0));
        pixels.setPixelColor(3, pixels.Color(255,0,0));
      } else {
        pixels.setPixelColor(0, pixels.Color(0,0,0));
        pixels.setPixelColor(1, pixels.Color(0,0,0));
        pixels.setPixelColor(2, pixels.Color(0,0,0));
        pixels.setPixelColor(3, pixels.Color(0,0,0));
      }
    }
  }
}

void setup() {
  #if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  //All initializations start. Turn on all LEDs to white
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(255, 255, 255));
  pixels.setPixelColor(1, pixels.Color(255, 255, 255));
  pixels.setPixelColor(2, pixels.Color(255, 255, 255));
  pixels.setPixelColor(3, pixels.Color(255, 255, 255));
  pixels.show();

  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  waActive = true; //will be off when connected to mqtt


  Serial.println("Read the configuration file.");
  readConfigurationFile();

  Serial.println("Configure WiFi");
  createCustomWiFiManager();

  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    writeConfigurationFile();
  }

  Serial.println("Configure MQTT");
  Serial.printf("MQTT: Server: %s port: %s\n", mqtt_server, mqtt_port);

  int p = atoi(mqtt_port);
  mqttClient.setServer(mqtt_server, p);
  mqttClient.setCredentials(mqtt_login, mqtt_password);
  mqttClient.setKeepAlive(30);
  mqttClient.setWill(MQTT_TOPIC_STATUS, 1, true, MQTT_STATUS_PAYLOAD_OFF); //topic, QoS, retain, payload

  Serial.println("MQTT: Set callbacks");
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);

  //Serial.println("MQTT: Connecting to broker...");
  //mqttClient.connect();
  /* Connect the MQTT client to the broker */
  int8_t attemptToConnectToMQTT = 0;
  Serial.println("MQTT: Connect to the broker");
  while ( (mqttClient.connected() == false) && (attemptToConnectToMQTT < 5) ) {
    if (WiFi.isConnected()) {
      Serial.printf("MQTT: Attempt %d. Connecting to broker ... \n", attemptToConnectToMQTT );
      mqttClient.connect();
    } else {
      //attemptToConnectToMQTT = 0;
      Serial.println("MQTT: WiFi is not connected. Try to reconnect WiFi.");
      WiFi.reconnect();
    }
    delay(3000);
    attemptToConnectToMQTT += 1;
  }

  /* If there is still no connection here, restart the device */
  if (!WiFi.isConnected()) {
    Serial.println("setup(): WiFi is not connected. Reset the device to initiate connection again.");
    ESP.restart();
  }

  if (!mqttClient.connected()) {
    Serial.println("setup(): The device is not connected to MQTT. Reset the device to initiate connection again.");
    ESP.restart();
  }

  // Animation
  waActive = false;
  errActive = false;

  queueInputCode.setPrinter(Serial);

  // All initializations are done. Turn off all LEDs
  pixels.setPixelColor(0, pixels.Color(0,0,0));
  pixels.setPixelColor(1, pixels.Color(0,0,0));
  pixels.setPixelColor(2, pixels.Color(0,0,0));
  pixels.setPixelColor(3, pixels.Color(0,0,0));
  pixels.show();

  timer.setInterval(INTERVAL_PUBLISH_STATE, publishState);
}


void loop() {
  timer.run();

  waitingAnimation();

  errorAnimation();

  // In case if the keypad is locked
  if ( (millis() > lock_endtime) && (lock_endtime > 0) ) {
    lock_endtime = 0;
    errActive = false;
  }

  if ( (!waActive) && (!errActive) ) {
    char key = keypad.getKey();

    if (key) {
      Serial.printf("Input symbol: [%c]\n", key);
      switch(key)
      {
        case '#':
          sendCode();
          break;
        case '*':
          queueInputCode.removeTail();
          break;
        default:
          queueInputCode.enqueue(key);
      }

      Serial.printf("  length: %d; Queue is full: %d\n", queueInputCode.count(), queueInputCode.isFull());
    }

    for (byte i = 0; i < DIGITS; i++ ) {
      bool isON = i < queueInputCode.count();
       if ( isON ) {
        pixels.setPixelColor(i, pixels.Color(0,150,0));
      } else {
        pixels.setPixelColor(i, pixels.Color(0,0,0));
      }
    }
  }

  pixels.show();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("loop(): WiFi is not connected. Reset the device to initiate connection again.");
    ESP.restart();
  }
}
