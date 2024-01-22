#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <stdio.h>
#include <time.h>
#include "influx.h"
#include "defines.h"

#include "debugConsole.h"

#define INFLUXDB_URL "http://rantanplan-ethernet:8086"
#define INFLUXDB_TOKEN "Zr0fsPmRgvNr0znkbudQNZBnGDHjkBOT41X4wJwZcoMMOAFVLy5eLtIpqlffQ966oQOD4aSmrTtdDX5LcVVu5Q=="
#define INFLUXDB_ORG "d727c1fb692f26f9"
#define INFLUXDB_BUCKET "energieJunkies"

static InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
// Data point
static Point sensor("boiler");
static Point sensor2("reboot");

bool influx_init()
{
    client.setInsecure();
    timeSync(EUROPE_VIENNA_TZ, NtpServer1, NtpServer2);
    DBGf("Init influx");
    // Check server connection
    if (client.validateConnection())
    {
        DBGf("Connected to InfluxDB: %s", client.getServerUrl().c_str());
    }
    else
    {
        DBGf("InfluxDB connection failed: %s", client.getLastErrorMessage().c_str());
        return false;
    }
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char buf[80];
    sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Report RSSI of currently connected network
    sensor2.addTag("device", "ES32");
    sensor2.clearFields();
    sensor2.addField("bootAt", buf);
    // Print what are we exactly writing
    Serial.print("Writing: ");
    client.pointToLineProtocol(sensor2);
    Serial.println(client.pointToLineProtocol(sensor2));
    if (!client.writePoint(sensor2))
    {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    return true;
}