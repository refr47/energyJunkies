# Loxone als Energiequelle

`LOXONE_API` aktiviert `LoxoneAsProvider`, eine `EnergySource` mit MQTT-Empfang.
Broker, Port und Zugangsdaten stammen aus den MQTT-Einstellungen des Setups;
`FEATURE_MQTT` und MQTT im Setup müssen aktiviert sein. Empfang und
Heizungs-Publishing teilen sich den vorhandenen `PubSubClient` in
`devices/mqtt.cpp`. Der Clock-Task bedient den MQTT-Loop einmal pro Sekunde
und abonniert die Energy-Topics auch dann, wenn Loxone erst nach dem
Broker-Verbindungsaufbau registriert wird. Nach jeder Verbindung werden die
Topics erneut abonniert. Die Registrierung läuft auch während einer
nächtlichen Pause des Energy-Pollings.

Die fünf Topic-Namen stehen als Konstanten in `src/provider/loxoneAsProvider.cpp`.
Die Vorgaben sind `fronius/p_grid_watt`, `fronius/p_load_watt`,
`fronius/p_pv_watt`, `fronius/p_batt_watt` und `fronius/p_socc_prozent`.
Sie lassen sich mit den Build-Flags
`LOXONE_TOPIC_P_GRID`, `LOXONE_TOPIC_P_LOAD`, `LOXONE_TOPIC_P_PV`,
`LOXONE_TOPIC_P_AKKU` und `LOXONE_TOPIC_SOC` überschreiben. Payloads sind
einfache Zahlen in Dezimalschreibweise:

| Topic-Ende | `ENERGY_STATE` | Einheit und Vorzeichen |
| --- | --- | --- |
| `p_grid_watt` | `p_grid` | W; positiv Bezug, negativ Einspeisung |
| `p_load_watt` | `p_load` | W; negativ Hausverbrauch |
| `p_pv_watt` | `p_pv` | W; PV-Produktion |
| `p_batt_watt` | `p_akku` | W; positiv Entladen, negativ Laden |
| `p_socc_prozent` | `soc` | %; 0 bis 100 |

Der Clock-Task speichert jede eingehende Änderung unter einem Semaphore. Nach
dem ersten empfangenen Wert liest der Energy-Task in jedem Zyklus eine Kopie
des Messsatzes. Noch nie empfangene Werte sind 0; danach bleibt jeder Wert bis
zur nächsten Nachricht auf seinem Topic erhalten. Auch unveränderte Werte
werden wie bei den anderen Providern erneut über `publishSample()` in
`energyState` übernommen. Es gibt keinen Timeout für unveränderte Werte.

Bei einem Verbindungsverlust schlägt `read()` fehl. Der Provider behält den
zuletzt empfangenen Messsatz und verwendet ihn nach einer Wiederverbindung
weiter; neue Nachrichten überschreiben einzelne Felder. Während einer
Unterbrechung verpasste Änderungen lassen sich ohne Initialabfrage nicht
nachholen.

Der Benutzer stellt über die Build-Flags sicher, dass genau eine Energiequelle
aktiviert ist. Die bestehende Auswahlreihenfolge in `ensureEnergySource()`
lautet Fronius, AMIS, Huawei, Loxone.
