# Loxone als Energiequelle

`LOXONE_API` aktiviert `LoxoneAsProvider`, eine `EnergySource` mit eigenem
MQTT-Empfang. Broker, Port und Zugangsdaten stammen aus den MQTT-Einstellungen
des Setups; MQTT muss dort aktiviert sein. Der Provider verwendet einen eigenen
MQTT-Client mit einer aus der MAC-Adresse gebildeten ID und verändert `devices/mqtt.cpp`
nicht.

Die fünf Topic-Namen stehen als Konstanten in `src/provider/loxoneAsProvider.cpp`.
Die vorläufigen, aus der Fronius-Struktur abgeleiteten Vorgaben sind
`site/P_Grid`, `site/P_Load`, `site/P_PV`, `site/P_Akku` und
`inverters/1/SOC`. Die tatsächlichen Loxone-Topics müssen dagegen geprüft
werden. Sie lassen sich mit den Build-Flags
`LOXONE_TOPIC_P_GRID`, `LOXONE_TOPIC_P_LOAD`, `LOXONE_TOPIC_P_PV`,
`LOXONE_TOPIC_P_AKKU` und `LOXONE_TOPIC_SOC` überschreiben. Payloads sind
einfache Zahlen in Dezimalschreibweise:

| Topic-Ende | `ENERGY_STATE` | Einheit und Vorzeichen |
| --- | --- | --- |
| `P_Grid` | `p_grid` | W; positiv Bezug, negativ Einspeisung |
| `P_Load` | `p_load` | W; negativ Hausverbrauch |
| `P_PV` | `p_pv` | W; PV-Produktion |
| `P_Akku` | `p_akku` | W; positiv Entladen, negativ Laden |
| `SOC` | `soc` | %; 0 bis 100 |

Der MQTT-Task speichert jede eingehende Änderung unter einem Semaphore. Sobald
alle fünf Werte mindestens einmal empfangen wurden, liest der Energy-Task in
jedem Zyklus eine Kopie des vollständigen Messsatzes. Auch unveränderte Werte
werden wie bei den anderen Providern erneut über `publishSample()` in
`energyState` übernommen. Es gibt keinen Timeout für unveränderte Werte.

Bei einem Verbindungsverlust schlägt `read()` fehl. Der Provider behält den
zuletzt vollständigen Messsatz und verwendet ihn nach einer Wiederverbindung
weiter; neue Nachrichten überschreiben einzelne Felder. Da die Nachrichten
nicht retained sind, muss beim Kaltstart jedes Topic mindestens einmal gesendet
werden, bevor `read()` erfolgreich sein kann. Während einer Unterbrechung
verpasste Änderungen lassen sich ohne Initialabfrage nicht nachholen.

Der Benutzer stellt über die Build-Flags sicher, dass genau eine Energiequelle
aktiviert ist. Die bestehende Auswahlreihenfolge in `ensureEnergySource()`
lautet Fronius, AMIS, Huawei, Loxone.
