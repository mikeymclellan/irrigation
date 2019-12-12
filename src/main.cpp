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

unsigned int relay_state = 0;

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
    digitalWrite(RELAY, relay_state);
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
//    wifiManager.autoConnect(DEVICE_NAME);
    wifiManager.autoConnect("NodeMCU-Arduino-PlatformIO");
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

/**
 * Setup OTA firmware updates via HTTP
 */
ESP8266WebServer httpServer(HTTP_SERVER_PORT);
ESP8266HTTPUpdateServer httpUpdater;

void setup_ota_firmware()
{
    httpUpdater.setup(&httpServer);
    httpServer.begin();

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

void publish_loop()
{
    unsigned long now = millis();

    if (now - lastMsg > 60000 * 60) {
        lastMsg = now;

        const size_t bufferSize = JSON_OBJECT_SIZE(15) + 20;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonObject &root = jsonBuffer.createObject();
        JsonObject &state = root.createNestedObject("state");
        JsonObject &reported = state.createNestedObject("reported");
        reported["a_x"] = 1;
        reported["a_y"] = 2;
        reported["a_z"] = 3;
        reported["relay_state"] = relay_state;

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

    // OTA updater
    httpServer.handleClient();
    MDNS.update();

    // Occasionally send a dummy MQTT message
    publish_loop();
}
