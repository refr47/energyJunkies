# Loxone als Energiequelle

`devices/loxoneAsProvider` implementiert `EnergySource`, wie `froniusHTTP`.
Der ESP32 fragt den Miniserver aktiv per HTTP GET ab. Es wird kein Wert
an einen virtuellen Eingang geschrieben; `boilerPower_watt` wird nicht verwendet.

## Konfiguration

- `-DLOXONE_API=1` kompiliert und aktiviert den Provider.
- `-DLOXONE='"10.0.0.210"'` legt den Miniserver-Host fest.
- `LOXONE_USER` und `LOXONE_PASSWD` verwenden dieselben Vorgaben wie der bisherige
  Loxone-Code, zentral in `include/www/loxoneConfig.h`. Beide sind über
  gleichnamige Build-Flags überschreibbar.
- Die bestehende Auswahlreihenfolge bleibt Fronius → AMIS → Huawei → Loxone.
  Für Loxone als einzige Quelle die Flags `FRONIUS_IV`, `AMIS_READER_DEV`
  und `HUAWEI_MODBUS` deaktivieren. Boot und Wiederanlauf verwenden dieselbe Reihenfolge.

## Controls auf dem Miniserver

Diese Namen müssen als abfragbare Ein-/Ausgänge in Loxone eingerichtet sein:

| Control | EnergySource-Feld | Einheit / Vorzeichen wie Fronius |
| --- | --- | --- |
| `P_Grid` | `p_grid` | W; positiv Bezug, negativ Einspeisung |
| `P_Load` | `p_load` | W; negativ Hausverbrauch |
| `P_PV` | `p_pv` | W; PV-Produktion |
| `P_Akku` | `p_akku` | W; positiv Entladen, negativ Laden |
| `SOC` | `soc` | %; 0 bis 100 |

Beispiel: `GET http://<LOXONE>/jdev/sps/io/P_Grid/state`, mit Basic Authentication
wie im bisherigen `loxone.cpp`. Erwartete Antwort:

```json
{"LL":{"control":"dev/sps/io/P_Grid","value":"-1200.5","Code":"200"}}
```

`Code` und `value` dürfen JSON-Zahlen oder numerische Strings sein.
Das JSON-Präfix `jdev` ist in der
[Loxone-Protokolldokumentation](https://www.loxone.com/dede/wp-content/uploads/sites/2/2021/03/1200_Communicating-with-the-Miniserver.pdf)
beschrieben; Statusabfragen in den
[Loxone-Webservices](https://www.loxone.com/dede/kb/webservices/).

Der Provider übernimmt dieselben fünf Regelungswerte wie `FroniusHTTP::read()`.
Die Fronius-Zusatzfelder `E_Day`, `E_Total`, `rel_Autonomy` und
`rel_SelfConsumption` gehören nicht zu `ENERGY_STATE` und werden nicht abgefragt.
Wie der Fronius-Adapter stellt diese Konfiguration Batteriedaten bereit;
`P_Akku` und `SOC` sind Pflichtwerte.

## Fehlerverhalten und Prüfung

Alle fünf Abfragen müssen erfolgreich sein (HTTP 200, LL.Code 200, endliche
Zahlen, gültiger SOC). Erst dann wird ein kompletter Messsatz über
`publishSample()` veröffentlicht. Ein Fehler verändert den letzten Messsatz
nicht und liefert `false` an die vorhandene Netzwerk-/Heizungsfehlerbehandlung.
Jede Anfrage hat 1500 ms Verbindungs- und Lese-Timeout.
Die Werte werden nacheinander gelesen; Loxone liefert dabei keinen atomaren
Snapshot aller Controls und keinen Messzeitstempel. Die Controls müssen in
Loxone daher fortlaufend aktualisiert werden.

Parser-Prüfung: `bash test/loxone/run.sh`.
Firmware-Build: `pio run -e lilygo-t-display-s3`.
