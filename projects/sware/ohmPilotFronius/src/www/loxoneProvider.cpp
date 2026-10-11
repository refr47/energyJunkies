/*
 * ──────────────────────────────────────────────────────
 *  Loxone Provider  –  /getLoxoneProvider
 *
 *  Liefert den aktuellen Stromverbrauch der Heizpatronen
 *  als JSON-Response:
 *        {"watt": <int>, "active": <bool>}
 *
 *  Berechnung basiert auf PID-Container (mAnalogOut,
 *  PID_PIN1, PID_PIN2) und Setup-Parameter
 *  (heizstab_leistung_in_watt / 3 ≈ 1 Phase).
 * ──────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "defines.h"
#include "loxoneProvider.h"
#include "ajaxCalls.h"

extern SemaphoreHandle_t g_ajaxMutex;

/* ─── lokale Watt-Berechnung ───────────────────── */

// einmalig pro Boot aus Setup berechnen (3-phasig)
static float wattPerPhase()
{
    static float cache = -1;
    if (cache < 0 && g_getDataCallback)
    {
        WEBSOCK_DATA d = g_getDataCallback();
        cache = d.setupData.heizstab_leistung_in_watt / 3.0f;
    }
    return cache;
}

static int calcWatt(WEBSOCK_DATA &d)
{
    float wph = wattPerPhase();
    if (wph <= 0) return 0;

    int w = (int)((float)d.pidContainer.mAnalogOut / 100.0f * wph);
    w += d.pidContainer.PID_PIN1 * wph
       + d.pidContainer.PID_PIN2 * wph;

    return w;
}

/* ─── öffentliche API ──────────────────────────── */

int loxoneProvider_getWatt()
{
    if (!g_getDataCallback) return 0;
    WEBSOCK_DATA d = g_getDataCallback();
    return calcWatt(d);
}

bool loxoneProvider_isActive(WEBSOCK_DATA &d)
{
    return calcWatt(d) > 0;
}

/* ─── HTTP-Handler-Registrierung ───────────────── */

void loxoneProvider_register(AsyncWebServer &server)
{
    server.on("/getLoxoneProvider", HTTP_GET,
              [](AsyncWebServerRequest *request)
    {
        // Mutex kurz, um Callback-Zeiger sicher zu lesen
        if (!g_getDataCallback ||
            xSemaphoreTake(g_ajaxMutex, pdMS_TO_TICKS(2000)) != pdTRUE)
        {
            request->send(503, "application/json",
                "{\"error\":503,\"watt\":0,\"active\":false}");
            return;
        }
        xSemaphoreGive(g_ajaxMutex);

        int watt = loxoneProvider_getWatt();

        char buf[96];
        snprintf(buf, sizeof(buf),
            "{\"watt\":%d,\"active\":%s}",
            watt, watt > 0 ? "true" : "false");

        request->send(200, "application/json", buf);
    });

    LOG_INFO(TAG_WEB, "loxoneProvider /getLoxoneProvider registered");
}
