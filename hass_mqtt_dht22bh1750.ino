#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

// Wifi: SSID and password
const char* WIFI_SSID = "SSID";
const char* WIFI_PASSWORD = "WPA2_PASSWORD";

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = "sensor_esp8266";
const PROGMEM char* MQTT_SERVER_IP = "192.168.X.X";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char* MQTT_USER = "MQTT_USER";
const PROGMEM char* MQTT_PASSWORD = "MQTT_PASSWORD";

const char* MQTT_PING_TOPIC = "/home/xxxx/light/ping";
const char* humidity_topic = "/home/xxxx/dht22/humidity";
const char* temperature_topic = "/home/xxxx/dht22/temperature";
const char* hic_topic = "/home/xxxx/dht22/hic";
const char* BH1750_LIGHT_TOPIC = "/home/xxxx/bh1750/lux";

volatile bool pong = false;

#define DHTTYPE DHT22
#define DHTPIN  14

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE, 22);
BH1750 lightMeter;

void setup() {
  
  pinMode(BUILTIN_LED, OUTPUT);
  
  //Serial.begin(115200);
  dht.begin();
  lightMeter.begin();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  // handle message topic
 if (String(MQTT_PING_TOPIC).equals(p_topic)) {
    pong = true;
  }
}

void reconnect() {
  ////Serial.print("INFO: Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    ////Serial.println("INFO: connected");
    // Once connected, publish an announcement...
    // ... and resubscribe
    client.subscribe(MQTT_PING_TOPIC);

  } else {
    ////Serial.print("ERROR: failed, rc=");
    ////Serial.print(client.state());
    ////Serial.println("DEBUG: try again in 5 seconds");
    // Wait 5 seconds before retrying
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.5;
float diffHum = 1.0;
float hic = 0.0;
uint16_t old_lux_value = 0;

volatile bool wifiStatus = true;
int ledState = LOW;
void wifiLED() {
  if (ledState == LOW)
    ledState = HIGH;
  else
    ledState = LOW;
  digitalWrite(BUILTIN_LED, ledState);
}

unsigned long previousMillis = 0;
long interval = 50;
bool non_blocking_interval() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

void loop() {

  //Blink LED when not connected, but wifi is on
  if (WiFi.status() != WL_CONNECTED && wifiStatus && non_blocking_interval()) {
    wifiLED();
  } else if (WiFi.status() == WL_CONNECTED && wifiStatus && client.connected()) {
    //Turn LED On (We are connected)
    
     if (!pong) {
      digitalWrite(BUILTIN_LED, HIGH);
    } else {
      pongWifiLED();
    }
    
    //udp.begin(localPort);
    client.loop();

  } else if (WiFi.status() == WL_CONNECTED && wifiStatus && non_blocking_interval() && !client.connected()) {
    wifiLED();
    reconnect(); //Reconnect MQTT if it fails
  } else if (!wifiStatus) {
    //Turn LED Off
    digitalWrite(BUILTIN_LED, LOW);
  }

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();
    float newHic = dht.computeHeatIndex(newTemp, newHum, false);

    if (checkBound(newTemp, temp, diff)) {
      temp = newTemp;
      //Serial.print("New temperature:");
      //Serial.println(String(temp).c_str());
      client.publish(temperature_topic, String(temp).c_str(), true);
    }

    if (checkBound(newHum, hum, diffHum)) {
      hum = newHum;
      //Serial.print("New humidity:");
      //Serial.println(String(hum).c_str());
      client.publish(humidity_topic, String(hum).c_str(), true);
    }

    if (checkBound(newHic, hic, diff)) {
      hic = newHic;
      //Serial.print("New hic:");
      //Serial.println(String(hic).c_str());
      client.publish(hic_topic, String(hic).c_str(), true);
    }

    uint16_t lux = lightMeter.readLightLevel();
    if (lux != old_lux_value) {
      old_lux_value = lux;
      client.publish(BH1750_LIGHT_TOPIC, String(lux).c_str(), true);
    }
  }
}

void pongWifiLED() {
  digitalWrite(BUILTIN_LED, HIGH);

  //delay 500 ms
  for (int i = 0; i < 10; i++) {
    delay(50);
    client.loop();
  }

  for (int i = 0; i < 3; i++) {
    digitalWrite(BUILTIN_LED, LOW);
    delay(50);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(50);
  }

  //delay 500 ms
  for (int i = 0; i < 10; i++) {
    delay(50);
    client.loop();
  }

  digitalWrite(BUILTIN_LED, LOW);
  pong = false;
}
