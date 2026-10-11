#pragma once

#include <ESPAsyncWebServer.h>

/**
 * @brief  Registriert den HTTP-GET-Handler unter "/getLoxoneProvider".
 */
void loxoneProvider_register(AsyncWebServer &server);

/**
 * @brief  Berechnet die aktuelle Heizpatronen-Leistung in Watt
 *         aus PID-Container + Setup-Parameter.
 * @return  0 wenn keine Daten verfügbar
 */
int loxoneProvider_getWatt();

/**
 * @brief  Liefert true, wenn die Heizpatronen aktuell aktiv sind
 *         (>0 Watt berechnet).
 *  @param  webSockD  Referenz auf den aktuellen Live-Daten-Snapshot
 * @return true wenn Watt > 0
 */
bool loxoneProvider_isActive(WEBSOCK_DATA &webSockD);
