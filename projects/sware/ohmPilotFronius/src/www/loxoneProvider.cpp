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

typedef struct _RET_CONT {
    int watt;
    int temp;
    bool active;
} RET_CONT;


extern SemaphoreHandle_t g_ajaxMutex;



static int wattPerPhase();
static void calcWatt(WEBSOCK_DATA& d, RET_CONT& ret);

/* ─── lokale Watt-Berechnung ───────────────────── */

// einmalig pro Boot aus Setup berechnen (3-phasig)
static int wattPerPhase()
{
    static int cache = -1;
    if (cache < 0 && g_getDataCallback)
    {
        WEBSOCK_DATA& d = g_getDataCallback();
        cache = d.setupData.heizstab_leistung_in_watt / 3;
    }
    return cache;
}

static void calcWatt(WEBSOCK_DATA &d, RET_CONT& ret)
{


    int wph = wattPerPhase();
    if (wph != 0) {
        ret.watt = (int)(d.pidContainer.mAnalogOut / 100 * wph);
        ret.watt += d.pidContainer.PID_PIN1 * wph
            + d.pidContainer.PID_PIN2 * wph;
        ret.temp = (d.temperature.sensor1 + d.temperature.sensor2) / 2;
        ret.active = (ret.watt > 0);
    }
    else {
        ret.watt = 0;
    }

    

}

/* ─── öffentliche API ──────────────────────────── */

void loxoneProvider_getWatt(RET_CONT& retContainer)
{
    if (!g_getDataCallback) return ;
    WEBSOCK_DATA &d = g_getDataCallback();;

     calcWatt( d, retContainer);
}



/* ─── HTTP-Handler-Registrierung ───────────────── */

void loxoneProvider_register(AsyncWebServer& server)
{

   
    server.on("/getLoxoneProvider", HTTP_GET,
        [](AsyncWebServerRequest* request)
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
            RET_CONT ret;
            memset(&ret, 0, sizeof(ret));
            loxoneProvider_getWatt(ret);

            char buf[96];
            snprintf(buf, sizeof(buf),
                "{\"watt\":%d,\"active\":%d,\"temperatur\":%d}",
                ret.watt, ret.active ? 1 : 0, ret.temp);

            request->send(200, "application/json", buf);
        });

    LOG_INFO(TAG_WEB, "loxoneProvider /getLoxoneProvider registered");
}
