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
}

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// WiFi / MQTT
WiFiClientSecure espClient;
PubSubClient client(aws_mqtt_server, 8883, callback, espClient); //set  MQTT port number to 8883 as per //standard
unsigned long lastMsg = 0;

void working_led()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
}

void setup_wifi()
{
    WiFiManager wifiManager;
    wifiManager.autoConnect("NodeMCU-Arduino-PlatformIO");
    Serial.println("Connected!");

    timeClient.begin();
    while(!timeClient.update()){
        timeClient.forceUpdate();
    }
    espClient.setX509Time(timeClient.getEpochTime());
    Serial.println("Time set");
}

/**
 * Setup OTA firmware updates via HTTP
 */
ESP8266WebServer httpServer(81);
ESP8266HTTPUpdateServer httpUpdater;

void setup_ota()
{
    MDNS.begin("irrigation-webupdate");

    httpUpdater.setup(&httpServer);
    httpServer.begin();

    MDNS.addService("http", "tcp", 81);
}

void setup_certs()
{
    if (!SPIFFS.begin())
    {
        Serial.println("[CERTS] Failed to mount file system");
        return;
    }

    Serial.print("[CERTS] Heap: ");
    Serial.println(ESP.getFreeHeap());

    // Load certificate file
    File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt with your uploaded file name
    if (!cert)
    {
        Serial.println("[CERTS] Failed to open cert file");
    }
    else
        Serial.println("[CERTS] Success to open cert file");

    delay(200);

    if (espClient.loadCertificate(cert))
        Serial.println("[CERTS] cert loaded");
    else
        Serial.println("[CERTS] cert not loaded");

    // Load private key file
    File private_key = SPIFFS.open("/private.der", "r"); //replace private with your uploaded file name
    if (!private_key)
    {
        Serial.println("[CERTS] Failed to open private cert file");
    }
    else
        Serial.println("[CERTS] Success to open private cert file");

    delay(200);

    if (espClient.loadPrivateKey(private_key))
        Serial.println("[CERTS] private key loaded");
    else
        Serial.println("[CERTS] private key not loaded");

    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r"); //replace ca with your uploaded file name
    if (!ca)
    {
        Serial.println("[CERTS] Failed to open ca ");
    }
    else
        Serial.println("[CERTS] Success to open ca");
    delay(200);

    if (espClient.loadCACert(ca))
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

    if (now - lastMsg > 10000) {
        lastMsg = now;

        const size_t bufferSize = JSON_OBJECT_SIZE(15) + 20;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonObject &root = jsonBuffer.createObject();
        JsonObject &state = root.createNestedObject("state");
        JsonObject &reported = state.createNestedObject("reported");
        reported["a_x"] = 1;
        reported["a_y"] = 2;
        reported["a_z"] = 3;

        String json_output;
        root.printTo(json_output);
        char payload[bufferSize];

        // Construct payload item
        json_output.toCharArray(payload, bufferSize);
        sprintf(payload, json_output.c_str());

        Serial.print("[AWS MQTT] Publish Message:");
        Serial.println(payload);
        client.publish(aws_mqtt_thing_topic_pub, payload);
    }
}

void aws_reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("[AWS] Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(aws_mqtt_client_id))
        {
            Serial.println("[AWS] connected");
            // ... and resubscribe
            client.subscribe(aws_mqtt_thing_topic_sub);
        }
        else
        {
            Serial.print("[AWS] failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying

            char buf[256];
            espClient.getLastSSLError(buf,256);
            Serial.print("WiFiClientSecure SSL error: ");
            Serial.println(buf);

            working_led();
            delay(5000);
        }
    }
}

void setup()
{
    // Debug LED
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(BAUD_RATE);
    delay(200);
    setup_wifi();
    delay(200);
    setup_ota();
    delay(200);
    setup_certs();
    delay(200);
    aws_reconnect();
}

void loop()
{
    if (!client.connected()) {

        aws_reconnect();
    }

//     Process any incoming messages
    client.loop();

//     OTA updater
    httpServer.handleClient();
    MDNS.update();

    // Occasionally send a dummy MQTT message
    publish_loop();
}
