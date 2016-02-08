 /**
 * Smart Letterbox
 *
 * Author: IBM IoT Australia
 * License: Apache License v2
 */

extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <Wire.h>
#include "DHT.h"
#include "SensitiveConfig_smartletterbox.h"
#include <Time.h>

os_timer_t myTimer;

// WiFI credentials

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// IBM Internet of Things MQTT configuration

char server[] = IOT_ORG IOT_BASE_URL;
char topic[] = "iot-2/evt/status/fmt/json";
char lettersensortopic[] = "iot-2/evt/status/lettersensor/json";
char authMethod[] = "use-token-auth";
char token[] = IOT_TOKEN;
char clientId[] = "d:" IOT_ORG ":" IOT_DEVICE_TYPE ":" IOT_DEVICE_ID;

// Global Variables

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

uint16_t volts;

// Temperature and Humidity Sensor (DHT22) Configuration

#define DHTPIN D2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

int counter = 0;
int lettersensorcounter = 0;
float h; // humidity
float t; // temperature
float hic; // heat index

// Sensor variables

int state = 0;

//*** Initialisation ********************************************


void init_wifi() {
  Serial.print("Connecting to ");
  Serial.print(ssid);

  if (strcmp (WiFi.SSID().c_str(), ssid) != 0) {
    WiFi.begin(ssid, password);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

// Initialise Sensor

void init_letter_sensor() {
  pinMode(D6, INPUT);
  attachInterrupt(D6, letterSensorActivated, CHANGE);
}

void letterSensorActivated() {
  state = 1;
}
// Initialisation

void setup() {
  // Configure serial port

  Serial.begin(115200);
  Serial.println();

  // Start the DHT22

  dht.begin();
  Serial.println("sensor is starting..");
  Serial.print("Reading Analog...");
  Serial.println(analogRead(0));

  init_wifi();
  init_letter_sensor();

//----------------- wifi_set_sleep_type(LIGHT_SLEEP_T);
//----------------- gpio_pin_wakeup_enable(GPIO_ID_PIN(2),GPIO_PIN_INTR_HILEVEL);
}

void letterSensorChanged() {
  state = 4;
  Serial.println("letter sensor activated");
  publishLetterSensorEvent();
  delay(2000);
  state = 0;
}

// *************************************************************

void loop() {
  switch (state) {
    case 0:
      delay(100);
      break;
    case 1:
      letterSensorChanged();
      break;
    default:
      break;
  }
  if (state == 0) {
    readDHTSensor();
    connectWithBroker();
    debugDisplayPayload();
    publishPayload();
    ++counter;
    delay(15000);
  }
}

// ************************************************************************

void readDHTSensor() {
   // reading DHT22

  h = dht.readHumidity();
  t = dht.readTemperature();

  // Check if we fail to read from the DHT sensor
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor");
    //TODO: Do more here
  }

  hic = dht.computeHeatIndex(t, h, false);
}

void connectWithBroker() {

  if (!!!client.connected()) {
    Serial.print("Reconnecting client to ");
    Serial.println(server);
     while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }
}

void publishLetterSensorEvent() {

  String payload = "{\"d\":{\"myName\":\"ESP8266.LetterSensorEvent\",\"counter\":";

  payload += lettersensorcounter;

  payload += ",\"status\":\"triggered\"}}";

  Serial.println("Sending payload letter sensor:");
  Serial.println(payload);

  if (client.publish(topic, (char*) payload.c_str())) {
    Serial.println("Published letter sensor event OK");
  } else {
    Serial.print("Publish failed with error:");
    Serial.println(client.state());
  }
}

void publishPayload() {

  String payload = "{\"d\":{\"myName\":\"ESP8266.Test1\",\"counter\":";

  payload += counter;
  payload += ",\"volts\":";
  payload += volts;
  payload += ",\"temperature\":";
  payload += t;
  payload += ",\"humidity\":";
  payload += h;
  payload += ",\"heatIndex\":";
  payload += hic;
  payload += "}}";

  Serial.print("Sending payload: ");
  Serial.println(payload);

  if (client.publish(topic, (char*) payload.c_str())) {
    Serial.println("Publish ok");
  } else {
    Serial.print("Publish failed with error:");
    Serial.println(client.state());
  }
}

void debugDisplayPayload() {
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.println(" *C ");
}

void callback(char* topic, byte* payload, unsigned int length) {
 Serial.println("callback invoked");
}
