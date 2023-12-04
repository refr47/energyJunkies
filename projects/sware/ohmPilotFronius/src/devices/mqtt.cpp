
#include <PubSubClient.h>
#include <WiFi.h>
#include "mqtt.h"
#include "debugConsole.h"

// Add your MQTT Broker IP address, example:
// const char* mqtt_server = "192.168.1.144";
static const char *mqtt_server = "10.0.0.2";
static WiFiClient espClient;
static PubSubClient client(espClient);
unsigned long lastMsg = 0;

#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

static void callback(char *topic, byte *payload, unsigned int length);
static void reconnect();

bool mqtt_init()
{
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    if (client.connect("Energy-Harvester" /*, "theUser", "nurEinTraum"*/))
    {
        client.publish("message/greetings", "Hi!");
        return true;
    }
    return false;
}

void mqtt_loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();
}

void mqtt_publish_pidParams(double kP, double kI, double kD)
{
    char buf[50];
    sprintf(buf, "%f", kP);
    client.publish("pid/p", buf);

    sprintf(buf, "%f", kI);
    client.publish("pid/i", buf);

    sprintf(buf, "%f ", kD);
    client.publish("pid/d", buf);
}

void mqtt_publish_en(int pwm, double availableWatt)
{
    char buf[50];

    sprintf(buf, "%d", pwm);
    client.publish("energy/pwm", buf);
    sprintf(buf, "%.2lf", availableWatt);
    client.publish("energy/available", buf);
}
/* ****************************/

static void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "Energy-Harvester";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("message/greetings", "Hi!");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1')
    {
        // digitalWrite(BUILTIN_LED, LOW); // Turn the LED on (Note that LOW is the voltage level
        //  but actually the LED is on; this is because
        //  it is active low on the ESP-01)
    }
    else
    {
        // digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off by making the voltage HIGH
    }
}