#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <stdio.h>
#include <time.h>
#include "influx.h"

#include "debugConsole.h"

#define INFLUXDB_URL INFLUX
#define INFLUXDB_TOKEN "Zr0fsPmRgvNr0znkbudQNZBnGDHjkBOT41X4wJwZcoMMOAFVLy5eLtIpqlffQ966oQOD4aSmrTtdDX5LcVVu5Q=="
#define INFLUXDB_ORG "d727c1fb692f26f9"
#define INFLUXDB_BUCKET "energieJunkies"

static InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
// Data point
static Point energy("energy");
static Point boiler("boiler");
static Point simulation("simulation");

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
    // sensor2.addTag("device", "ES32");
    Point environment("environment");
    environment.addField("device", "esp32");
    environment.addField("inverter", "fronius");
    environment.addField("model", "gen24");
    environment.addField("bootAt", buf);
    // Print what are we exactly writing

    client.pointToLineProtocol(environment);
    DBGf("Write to influx: %s", client.pointToLineProtocol(environment).c_str());
    if (!client.writePoint(environment))
    {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    return true;
}

bool influx_write(WEBSOCK_DATA &webSockData)
{
    energy.clearFields();
    boiler.clearFields();
#ifdef FRONIUS_API
    if (webSockData.setup.externerSpeicher)
    {
        energy.addField("akku", FRONIUS.p_akku);
    }

    energy.addField("grid", FRONIUS.p_grid);
    energy.addField("load", FRONIUS.p_load);
    energy.addField("pv", FRONIUS.p_pv);
    energy.addField("availableWatt", (FRONIUS.p_pv + FRONIUS.p_akku + FRONIUS.p_load));

#else
    energy.addField("grid", METER_DATA.acCurrentPower);
    energy.addField("load", (INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower);
    energy.addField("pv",INVERTER_DATA.acCurrentPower);
     energy.addField("availableWatt",INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower);
#endif
    energy.addField("pwm", webSockData.pidContainer.mAnalogOut);
    energy.addField("relay1", webSockData.pidContainer.PID_PIN1);
    energy.addField("relay2", webSockData.pidContainer.PID_PIN2);

    boiler.addField("temperature1", webSockData.temperature.sensor1);
    boiler.addField("temperature2", webSockData.temperature.sensor2);

    String proto = client.pointToLineProtocol(energy);
    if (!client.writePoint(energy))
    {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    proto = client.pointToLineProtocol(boiler);
    if (!client.writePoint(boiler))
    {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}

bool influx_write_test(double availableData,double availableDataAfter, WEBSOCK_DATA &webSockData)
{
    simulation.clearFields();
    simulation.addField("pwm", webSockData.pidContainer.mAnalogOut);
    simulation.addField("relay1", webSockData.pidContainer.PID_PIN1);
    energy.addField("relay2", webSockData.pidContainer.PID_PIN2);
    energy.addField("availAbleWattBefor",availableData);
    energy.addField("availAbleWattAfter",availableDataAfter);
      String proto = client.pointToLineProtocol(simulation);
    if (!client.writePoint(simulation))
    {
        Serial.print("InfluxDB write failed (simulation): ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}