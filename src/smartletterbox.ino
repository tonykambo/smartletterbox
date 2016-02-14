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
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <Wire.h>
#include "DHT.h"
#include "SensitiveConfig_smartletterbox.h"
#include <Time.h>

os_timer_t myTimer;
bool timerCompleted;

// WiFI credentials

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// IBM Internet of Things MQTT configuration

char server[] = IOT_ORG IOT_BASE_URL;
char topic[] = "iot-2/evt/tempsensor/fmt/json";
char lettersensortopic[] = "iot-2/evt/lettersensor/fmt/json";
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

#define LETTER_BOTTOM_SENSOR 1
#define LETTER_TOP_SENSOR 2

// Initially set to IDLE state

enum SystemState {
  IDLE,
  LETTER_SENSOR_DETECTED,
  PUBLISH_SENSOR_EVENT
};

int letterSensorDetected = 0;

SystemState state = IDLE;

//*** Initialisation ********************************************


void init_wifi() {
  Serial.println("Initialising Wifi");
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to ");
  Serial.print(ssid);
  Serial.print(password);

  if (strcmp (WiFi.SSID().c_str(), ssid) != 0) {
    WiFi.begin(ssid, password);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected with OTA, IP address: ");
  Serial.println(WiFi.localIP());

// Port defaults to 8266
// ArduinoOTA.setPort(8266);

// Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(IOT_DEVICE_ID);

// No authentication by default
// ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA update");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnding OTA update");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.print("Test: ");
  Serial.println(WiFi.localIP());

}

// Initialise Letter Sensors

void init_letter_sensor() {
  Serial.println("Initialising letter sensor");
  pinMode(D6, INPUT);
  pinMode(D7, OUTPUT);
  pinMode(D4, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  attachInterrupt(D6, letterBottomSensorActivated, FALLING);
  attachInterrupt(D4, letterTopSensorActivated, FALLING);
  digitalWrite(D7, LOW);
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

  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 15000, true);

//----------------- wifi_set_sleep_type(LIGHT_SLEEP_T);
//----------------- gpio_pin_wakeup_enable(GPIO_ID_PIN(2),GPIO_PIN_INTR_HILEVEL);
}


// *************************************************************

void loop() {
  switch (state) {
    // case IDLE:
    //   delay(100);
    //   break;
    case LETTER_SENSOR_DETECTED:
      digitalWrite(D7, HIGH);
      connectWithBroker();
      letterSensorChanged();
      break;
    default:
      break;
  }

  if (timerCompleted == true) {
    digitalWrite(D7, HIGH);
    readDHTSensor();
    connectWithBroker();
    //   debugDisplayPayload();
    publishPayload();
    ++counter;
    delay(2000);
    digitalWrite(D7, LOW);
    timerCompleted = false;
  }

  // if (state == IDLE) {
  //   readDHTSensor();
  //   connectWithBroker();
  //   debugDisplayPayload();
  //   publishPayload();
  //   ++counter;
  //   delay(5000);
  // }
  ArduinoOTA.handle();
  yield();
}

// timer

void timerCallback(void *pArg) {

  timerCompleted = true;



}

// Letter Sensors

void letterSensorChanged() {
  state = PUBLISH_SENSOR_EVENT;
  if (letterSensorDetected == LETTER_BOTTOM_SENSOR) {
    Serial.print("Bottom ");
  } else {
    Serial.print("Top ");
  }
  Serial.println("letter sensor activated");
  publishLetterSensorEvent(letterSensorDetected);
  delay(2000);
  digitalWrite(D7, LOW);
  state = IDLE;
}


void letterBottomSensorActivated() {
  state = LETTER_SENSOR_DETECTED;
  letterSensorDetected = LETTER_BOTTOM_SENSOR;
}

void letterTopSensorActivated() {
  state = LETTER_SENSOR_DETECTED;
  letterSensorDetected = LETTER_TOP_SENSOR;
}


// DHT Sensor

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

// MQTT Broker Connection and Publishing

void connectWithBroker() {
  Serial.print("Checking broker connection..");

  if (!!!client.connected()) {

    Serial.print("\nReconnecting client to ");
    Serial.println(server);

    while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  } else {
      Serial.println("CONNECTED");
  }
}

void publishLetterSensorEvent(int letterSensorDetected) {

  String payload = "{\"d\":{\"Type\":";

  if (letterSensorDetected == LETTER_BOTTOM_SENSOR) {
    payload += "Bottom";
  } else {
    payload += "Top";
  }
  payload += "LetterSensorOTA1\",\"counter\":";
//  String payload = "{\"d\":{\"Type\":\"LetterSensor\",\"counter\":";

  payload += lettersensorcounter;

  payload += ",\"status\":\"ON\"}}";

  Serial.println("Sending payload letter sensor:");
  Serial.println(payload);

//if (client.publish(topic, (char*) payload.c_str())) {
  if (client.publish(lettersensortopic, (char*) payload.c_str())) {
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


//   String payload = "{\"d\":{\"Type\":";
//
//   if (letterSensorDetected == LETTER_BOTTOM_SENSOR) {
//     payload += "Bottom";
//   } else {
//     payload += "Top";
//   }
//   payload += "TSLetterSensorOTA1\",\"counter\":";
// //  String payload = "{\"d\":{\"Type\":\"LetterSensor\",\"counter\":";
//
//   payload += lettersensorcounter;
//
//   payload += ",\"status\":\"ON\"}}";





  Serial.print("Sending payload: ");
  Serial.println(payload);

  if (client.publish(topic, (char*) payload.c_str())) {
    Serial.println("Publish ok");
  } else {
    Serial.print("Publish failed with error:");
    Serial.println(client.state());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
 Serial.println("callback invoked");
}

// Debug

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
