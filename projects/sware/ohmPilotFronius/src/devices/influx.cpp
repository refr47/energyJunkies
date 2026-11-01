#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <stdio.h>
#include <time.h>
#include "influx.h"
#include "eprom.h"
#include "app_state.h"
#include "hotUpdate.h"
#include <atomic>

#include "debugConsole.h"
static InfluxDBClient client;
static bool influxEnabled = false;
// Data point
static Point energy("energy");
static Point boiler("boiler");
static Point simulation("monitor");
static Point inverter("inverter");
static Point means("means");

// static bool influxAvailable = false;
//  static bool *isInfluxValid = &influxAvailable;
static CALLBACK_GET_DATA webSockData;

static INFLUX_SETUP influxConfig{};
static bool connected = false;
static bool attempted = false;
static uint32_t lastAttempt = 0;
static std::atomic<uint32_t> applied{0};
uint32_t influx_appliedRevision() { return applied.load(); }

bool influx_init(CALLBACK_GET_DATA getData)
{
    webSockData = getData;
    return false; // The energy task owns configuration, connections and writes.
}

void influx_service()
{
    INFLUX_SETUP next;
    bool enabled;
    uint32_t revision;
    if (!appLockConfig(50)) return;
    next = g_app.webSockData.setupData.influxSetup;
    enabled = g_app.webSockData.setupData.influxEnabled;
    revision = setupAppliedRevision();
    appUnlockConfig();
    if (enabled != influxEnabled || memcmp(&next, &influxConfig, sizeof(next))) {
        influxConfig = next;
        influxEnabled = enabled;
        connected = attempted = false;
        if (enabled) {
            client.setConnectionParams(next.influxHost, next.influxOrg, next.influxBucket, next.influxToken);
            client.setInsecure();
        }
    }
    applied.store(revision);
    if (!enabled || WiFi.status() != WL_CONNECTED) connected = false;
    else if (!connected && (!attempted || uint32_t(millis() - lastAttempt) >= 30000)) {
        lastAttempt = millis();
        attempted = true;
        connected = client.validateConnection();
    }
    if (appLockData(50)) {
        g_app.webSockData.states.influx = connected;
        appUnlockData();
    }
}

bool influx_write(WEBSOCK_DATA &webSockData)
{
    if (!influxEnabled || !connected) return false;

    if (!webSockData.states.influx || !webSockData.states.networkOK)
        return false;

    energy.clearFields();
    boiler.clearFields();
    // ── energyState ist quelle-agnostisch befüllt ──
    if (webSockData.setupData.akku)
    {
        energy.addField("akku", webSockData.energyState.p_akku);
    }
    energy.addField("grid", webSockData.energyState.p_grid);
    energy.addField("load", webSockData.energyState.p_load);
    energy.addField("pv", webSockData.energyState.p_pv);
    energy.addField("availableWatt",
        webSockData.energyState.p_pv + webSockData.energyState.p_akku + webSockData.energyState.p_load);
    energy.addField("pwm", webSockData.pidContainer.mAnalogOut);
    energy.addField("relay1", webSockData.pidContainer.PID_PIN1);
    energy.addField("relay2", webSockData.pidContainer.PID_PIN2);

    boiler.addField("temperature1", webSockData.temperature.sensor1);
    boiler.addField("temperature2", webSockData.temperature.sensor2);

    String proto = client.pointToLineProtocol(energy);
    if (!client.writePoint(energy))
    {
        connected = false;
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    proto = client.pointToLineProtocol(boiler);
    if (!client.writePoint(boiler))
    {
        connected = false;
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}

bool influx_write_test(double boilerData, double availableWatt)
{
    if (!influxEnabled || !connected) return false;
    WEBSOCK_DATA data = webSockData();
    if (!data.states.influx || !data.states.networkOK)
        return false;
    /* DBGf("influx_write_test BEGIN, isValid: %d", *isInfluxValid); */

    simulation.clearFields();
    simulation.addField("pwm", data.pidContainer.mAnalogOut);
    simulation.addField("relay1", data.pidContainer.PID_PIN1);
    simulation.addField("relay2", data.pidContainer.PID_PIN2);
    simulation.addField("storage", boilerData);
    simulation.addField("availableWatt", availableWatt);
    simulation.addField("ForceHeating", data.setupData.forceHeating);
    String proto = client.pointToLineProtocol(simulation);
    if (!client.writePoint(simulation))
    {
        connected = false;
        Serial.print("InfluxDB write failed (simulation): ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}

bool influx_write_production(double pv, double akku, double load)
{
    if (!influxEnabled || !connected) return false;
    // DBGf("influx_write_production BEGIN");
    WEBSOCK_DATA data = webSockData();
    if (!data.states.influx || !data.states.networkOK)
        return false;

    inverter.clearFields();
    inverter.addField("pv", pv);
    inverter.addField("akku", akku);
    inverter.addField("load", load);

    String proto = client.pointToLineProtocol(inverter);
    if (!client.writePoint(inverter))
    {
        connected = false;
        Serial.print("InfluxDB write failed (inverter): ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}

// static Point means("means");

bool influx_write_mean_val(double watt)
{
    if (!influxEnabled || !connected) return false;
    WEBSOCK_DATA data = webSockData();
    if (!data.states.influx || !data.states.networkOK)
        return false;

    means.clearFields();
    means.addField("availableW", watt);
    String proto = client.pointToLineProtocol(means);
    if (!client.writePoint(means))
    {
        connected = false;
        Serial.print("InfluxDB write failed (means): ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}

bool influx_write_mean(double mean)
{
    if (!influxEnabled || !connected) return false;
    WEBSOCK_DATA data = webSockData();
    if (!data.states.influx || !data.states.networkOK)
        return false;

    means.clearFields();
    means.addField("availableW", mean);
    String proto = client.pointToLineProtocol(means);
    if (!client.writePoint(means))
    {
        connected = false;
        Serial.print("InfluxDB write failed (means): ");
        Serial.println(client.getLastErrorMessage());
        return false;
    }
    return true;
}