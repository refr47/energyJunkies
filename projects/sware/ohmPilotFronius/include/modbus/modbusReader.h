#pragma once

#include "defines.h"

/**
 * @brief  Initialisiert die Modbus/TCP-Verbindung zum Fronius-Inverter.
 *         Liest die Inverter-IP aus &lt;Setup&gt; und verbindet.
 * @param  setup  Setup-Struktur mit Inverter-IP-Adresse
 * @return true bei erfolgreicher Verbindung
 */
bool mb_init(Setup &setup);

/**
 * @brief  Liest die Inverter-Summary-Werte per Modbus/TCP:
 *          - AC current power (kW)
 *          - AC total energy
 *          - DC current power
 * @param  setup      Setup-Struktur mit Inverter-IP
 * @param  container  MB_CONTAINER, in den die Resultate geschrieben werden
 * @return true bei erfolgreichem Read + Scale
 */
bool mb_readInverter(Setup &setup, MB_CONTAINER &container);
