#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "FS.h"
#include "main.h"

#define RELAY D3
#define BAUD_RATE 115200
#define HTTP_SERVER_PORT 80
#define MQTT_PORT 8883
#define DEVICE_NAME "Irrigation"

// The current relay state. Our relay is inverse logic
unsigned int relay_state = 1;

// How many seconds the relay has been requested to be on for
unsigned long relay_on_timer = 0;

// When the relay should be turned off
unsigned long relay_off_time = 0;

void publish_state();

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("[AWS] Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    const size_t bufferSize = JSON_OBJECT_SIZE(15) + 20;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& obj = jsonBuffer.parseObject(payload);

    relay_state = obj["state"]["desired"]["relay_state"] | relay_state;

    if (obj["state"]["desired"]["relay_on_timer"]) {
        Serial.print("Setting `relay_on_timer` for ");
        Serial.print((int)obj["state"]["desired"]["relay_on_timer"]);
        Serial.println();
        relay_on_timer = (int)obj["state"]["desired"]["relay_on_timer"];
        relay_off_time = millis() + relay_on_timer;
    }
}

// MQTT
WiFiClientSecure sslClient;
PubSubClient mqttClient(aws_mqtt_server, MQTT_PORT, callback, sslClient);
unsigned long lastMsg = 0;

void working_led()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
}

void setup_ntp()
{
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org");

    timeClient.begin();
    while (!timeClient.update()) {
        timeClient.forceUpdate();
    }

    // We need accurate time to validate SSL certificates
    sslClient.setX509Time(timeClient.getEpochTime());
    Serial.println("Time set");
}

void setup_wifi_manager()
{
    WiFiManager wifiManager;
    wifiManager.autoConnect(DEVICE_NAME);
    Serial.println("Connected!");
}

void setup_wifi() {
  WiFi.begin("McLellan", "minceontoast");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void check_wifi() {
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }
}

/**
 * Setup OTA firmware updates via HTTP
 */
ESP8266WebServer httpServer(HTTP_SERVER_PORT);
ESP8266HTTPUpdateServer httpUpdater;

void handleRoot()
{
  httpServer.send(200, "text/plain", "hello from esp8266!\r\n");
}

void setup_ota_firmware()
{
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    httpServer.on("/", handleRoot);

    // Advertise our firmware HTTP server with Multicast DNS
    MDNS.begin(DEVICE_NAME);
    MDNS.addService("http", "tcp", HTTP_SERVER_PORT);
}

void setup_certs()
{
    if (!SPIFFS.begin()) {
        Serial.println("[CERTS] Failed to mount file system");
        return;
    }

    Serial.print("[CERTS] Heap: ");
    Serial.println(ESP.getFreeHeap());

    // Load certificate file
    File cert = SPIFFS.open("/cert.der", "r");
    if (!cert)
        Serial.println("[CERTS] Failed to open cert file");
    else
        Serial.println("[CERTS] Success to open cert file");

    if (sslClient.loadCertificate(cert))
        Serial.println("[CERTS] cert loaded");
    else
        Serial.println("[CERTS] cert not loaded");

    // Load private key file
    File private_key = SPIFFS.open("/private.der", "r");
    if (!private_key)
        Serial.println("[CERTS] Failed to open private cert file");
    else
        Serial.println("[CERTS] Success to open private cert file");

    if (sslClient.loadPrivateKey(private_key))
        Serial.println("[CERTS] private key loaded");
    else
        Serial.println("[CERTS] private key not loaded");

    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r");
    if (!ca)
        Serial.println("[CERTS] Failed to open ca ");
    else
        Serial.println("[CERTS] Success to open ca");

    if (sslClient.loadCACert(ca))
        Serial.println("[CERTS] ca loaded");
    else
        Serial.println("[CERTS] ca failed");

    Serial.print("[CERTS] Heap: ");
    Serial.println(ESP.getFreeHeap());
    working_led();
}

void publish_state()
{
    const size_t bufferSize = JSON_OBJECT_SIZE(15) + 20;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.createObject();
    JsonObject &state = root.createNestedObject("state");
    JsonObject &reported = state.createNestedObject("reported");
    reported["relay_state"] = relay_state;
    reported["relay_on_timer"] = relay_on_timer;

    String json_output;
    root.printTo(json_output);
    char payload[bufferSize];

    // Construct payload item
    json_output.toCharArray(payload, bufferSize);
    sprintf(payload, json_output.c_str());

    Serial.print("[AWS MQTT] Publish Message:");
    Serial.println(payload);
    mqttClient.publish(aws_mqtt_thing_topic_pub, payload);
}

void relay_loop()
{
    unsigned long now = millis();

    if (relay_off_time && now > relay_off_time) {
        relay_on_timer = 0;
        relay_off_time = 0;
        relay_state = 1;
        Serial.println("Relay on timer ended");
    }

    if (relay_off_time && now < relay_off_time && relay_state == 1) {
        relay_state = 0;
        Serial.println("Relay on timer started ");
    }

    if (relay_state != digitalRead(RELAY)) {
        digitalWrite(RELAY, relay_state);
        publish_state();
    }
}

void aws_reconnect()
{
    while (!mqttClient.connected()) {
        Serial.print("[AWS] Attempting MQTT connection...");
        if (mqttClient.connect(aws_mqtt_client_id)) {
            Serial.println("[AWS] connected");
            mqttClient.subscribe(aws_mqtt_thing_topic_sub);
        } else {
            Serial.print("[AWS] failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");

            char buf[256];
            sslClient.getLastSSLError(buf,256);
            Serial.print("WiFiClientSecure SSL error: ");
            Serial.println(buf);
            working_led();
            delay(5000);
        }
    }
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, LOW);

    Serial.begin(BAUD_RATE);
    setup_wifi();
    setup_ntp();
    setup_ota_firmware();
    setup_certs();
}

void loop()
{
    if (!mqttClient.connected()) {
        aws_reconnect();
    }

    // Process any incoming MQTT messages
    mqttClient.loop();

    relay_loop();

    // OTA updater
    httpServer.handleClient();
    MDNS.update();

    check_wifi();
    // Occasionally send a dummy MQTT message
//    publish_loop();
}
