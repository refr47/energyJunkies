
#ifdef MQTT

#include <PubSubClient.h>
#include <WiFi.h>
#include "mqtt.h"
#include "debugConsole.h"

// Add your MQTT Broker IP address, example:
// const char* mqtt_server = "192.168.1.144";
static const char *mqtt_server = MQTT;
static WiFiClient espClient;
static PubSubClient client(espClient);
unsigned long lastMsg = 0;

#define TOPIC_MESSAGE_GREETING "message/greetings"
#define TOPIC_LOG_W "log/logw"
#define TOPIC_LOG_I "log/logi"
#define TOPIC_LOG_D "log/logd"
#define TOPIC_LOG_E "log/loge"

#define TOPIC_PWM "energy/pwm"
#define TOPIC_ENERGY_AVAIL "energy/available"
#define TOPIC_MODBUS_RECONNECT "modbus/connection/reconnect"
#define TOPIC_MODBUS_WRONG_PRODUCTION_VAL "modbus/error/currentProduction"
#define TOPIC_MODBUS_INVERTER_PRODUCTION "modbus/data/currentProduction"
#define TOPIC_MODBUS_SMART_METER_IN_EXPORT "modbus/data/inExport"
#define TOPIC_MODBUS_VERBRAUCH "modbus/data/verbrauch"

#define TOPIC_ALARM_TEMP_S1 "alarm/sensor/s1"
#define TOPIC_ALARM_TEMP_S2 "alarm/sensor/s2"

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
        client.publish(TOPIC_MESSAGE_GREETING, "Init Energy Harvester");
        return true;
    }
    return false;
}

void mqtt_loop()
{
    if (!client.connected())
    {
         if (WiFi.status() == WL_CONNECTED ) {
            LOG_INFO("mqtt_loop::mqtt_loop, reconnect, wifi ok, mqtt-server failed.");
             reconnect();
         }
        
    }
    client.loop();
}

void mqtt_publish_en(int pwm, double availableWatt)
{
    char buf[50];

    sprintf(buf, "%d", pwm);
    client.publish(TOPIC_PWM, buf);
    sprintf(buf, "%.2lf", availableWatt);
    client.publish(TOPIC_ENERGY_AVAIL, buf);
}

void mqtt_publish_alarm_temp(int tempS1, int tempS2)
{
    char buf[30];

    sprintf(buf, "%d", tempS1);
    client.publish(TOPIC_ALARM_TEMP_S1, buf);
    sprintf(buf, "%d", tempS2);
    client.publish(TOPIC_ALARM_TEMP_S2, buf);
}
void mqtt_publish_modbus_reconnect(const char *ipInverter)
{
    client.publish(TOPIC_MODBUS_RECONNECT, ipInverter);
}

void mqtt_publish_log( char token,const char *logM)
{
    if (token == 'W')
        client.publish(TOPIC_LOG_W, logM);
    if (token=='I')
        client.publish(TOPIC_LOG_I, logM);
    if (token == 'D')
        client.publish(TOPIC_LOG_D, logM);
    if (token == 'E')
        client.publish(TOPIC_LOG_E, logM);
}

void mqtt_publish_modbus_wrong_production_val(double readVal)
{
    char buf[30];

    sprintf(buf, "%.2lf", readVal);
    client.publish(TOPIC_MODBUS_WRONG_PRODUCTION_VAL, buf);
}
/*
void mqtt_publish_modbus_current_state(MB_CONTAINER &modb)
{
    char buf[30];
    sprintf(buf, "%.2lf", modb.inverterSumValues.data.acCurrentPower);
    client.publish(TOPIC_MODBUS_INVERTER_PRODUCTION, buf);
    sprintf(buf, "%.2lf", modb.meterValues.data.acCurrentPower);
    client.publish(TOPIC_MODBUS_SMART_METER_IN_EXPORT, buf);
    sprintf(buf, "%.2lf", modb.inverterSumValues.data.acCurrentPower + modb.meterValues.data.acCurrentPower);
    client.publish(TOPIC_MODBUS_VERBRAUCH, buf);
}

*/
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

#endif