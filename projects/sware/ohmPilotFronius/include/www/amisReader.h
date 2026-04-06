#pragma once

#ifdef AMIS_READER_DEV

#include <Arduino.h>

#include "defines.h"

bool amisReader_initRestTargets(WEBSOCK_DATA &setup);
bool amisReader_readRestTarget(WEBSOCK_DATA &);

#endif

/*
Feld-Nr. 	Bezeichnung	Bedeutung / Einheit	Entsprechender OBIS-Code
Field 1	Energie A+	Gesamter Strombezug (Zählerstand) in kWh	1.8.0
Field 2	Energie A-	Gesamte Einspeisung (z. B. PV-Anlage) in kWh	2.8.0
Field 3	Energie R+	Positive Blindenergie in kvarh	3.8.x
Field 4	Energie R-	Negative Blindenergie in kvarh	4.8.x
Field 5	Mom. Wirk. P+	Aktueller Verbrauch (Bezug) in Watt (W)	1.7.0
Field 6	Mom. Wirk. P-	Aktuelle Erzeugung (Einspeisung) in Watt (W)	2.7.0
Field 7	Blind. Q+	Aktuelle positive Blindleistung in var	-
Field 8	Blind. Q-	Aktuelle negative Blindleistung in var

*/