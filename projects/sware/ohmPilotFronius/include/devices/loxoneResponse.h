#pragma once

// Loxone /jdev Antwort: LL.Code und LL.value müssen gültig sein.
// Bei Fehler bleibt value unverändert.
bool loxoneParseValue(const char* payload, double &value);
