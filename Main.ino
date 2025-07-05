#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"  // Contains WIFI_SSID, WIFI_PASSWORD, cacert, client_cert, privkey

#define ONBOARD_LED LED_BUILTIN  // GPIO2
#define D1_LED_PIN 5             // GPIO5 (D1)

WiFiClientSecure net;
PubSubClient client(net);

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = 8883;
const char* mqtt_topic = "esp8266/board";
const char* client_id = "ESP8266BoardClient";

// --- Time Sync ---
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synchronized!");
}

// --- MQTT Callback ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  // Parse JSON payload
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }

  const char* cmd = doc["message"];
  Serial.println(cmd);
  String message = String(cmd);

  if (message == "on") {
    digitalWrite(ONBOARD_LED, LOW);  // Active-low
    digitalWrite(D1_LED_PIN, LOW);
    Serial.println("Both LEDs ON");
  } 
  else if (message == "off") {
    digitalWrite(ONBOARD_LED, HIGH);
    digitalWrite(D1_LED_PIN, HIGH);
    Serial.println("Both LEDs OFF");
  } 
  else if (message.startsWith("blink:")) {
    int count = message.substring(6).toInt();
    Serial.print("Blinking "); Serial.print(count); Serial.println(" times...");
    for (int i = 0; i < count; i++) {
      digitalWrite(ONBOARD_LED, LOW);
      digitalWrite(D1_LED_PIN, LOW);
      delay(300);
      digitalWrite(ONBOARD_LED, HIGH);
      digitalWrite(D1_LED_PIN, HIGH);
      delay(300);
    }
    Serial.println("Done blinking");
  } 
  else {
    Serial.println("Unknown command");
  }
}

// --- Connect to AWS ---
void connectAWS() {
  while (!client.connected()) {
    Serial.println("Connecting to AWS IoT...");
    if (client.connect(client_id)) {
      Serial.println("Connected to AWS IoT");
      client.subscribe(mqtt_topic);
      Serial.print("Subscribed to topic: ");
      Serial.println(mqtt_topic);
    } else {
      Serial.print("MQTT connect failed, rc=");
      Serial.print(client.state());
      Serial.println(" -> retrying in 5s");
      delay(5000);
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("\nESP Booting...");

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(D1_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH);  // OFF
  digitalWrite(D1_LED_PIN, HIGH);   // OFF

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  syncTime();

  net.setTrustAnchors(new BearSSL::X509List(cacert));
  net.setClientRSACert(
    new BearSSL::X509List(client_cert),
    new BearSSL::PrivateKey(privkey)
  );

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// --- Loop ---
void loop() {
  if (!client.connected()) {
    connectAWS();
  }
  client.loop();
}
