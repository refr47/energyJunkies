# Heizplan, Notfallgrenze und lokaler Viertelstundenverlauf

Der optionale Heizplan ist standardmäßig ausgeschaltet. In diesem Zustand bleibt
`heizstab_temp_min` die sofort wirksame Mindesttemperatur. Bei eingeschaltetem
Plan ist dieser Wert die Zieltemperatur des Tarifintervalls. Die separate
`heating_plan_emergency_c` gilt jederzeit, auch außerhalb des Intervalls und
bei ungültiger Uhrzeit. Sie muss unter der Zieltemperatur liegen. Sensorfehler
und die absolute Temperaturgrenze schalten weiterhin alle Heizpfade ab.

Die lokalen Minuten `heating_plan_start_minute` und `heating_plan_end_minute`
liegen zwischen 0 und 1439; ein Fenster darf über Mitternacht reichen. Gleiche
Zeiten werden bei aktivem Plan abgelehnt. Uhrzeit und Sommerzeit folgen der am
ESP eingestellten lokalen Zeitzone. Alte Schema-2-NVS-Snapshots erhalten beim
Lesen einen ausgeschalteten Heizplan; bestehendes Heizverhalten bleibt erhalten.

`heating_plan_boiler_liters` ist das geschätzte wirksame Wasservolumen. Die
Planung schätzt den Wärmebedarf als `Liter × (Zieltemperatur − Temperatur) ×
1,163 Wh` und die Heizdauer mit 20 % Zeitreserve aus der konfigurierten
Heizleistung. Das ist keine Zusage: Wasserschichtung, Sensorposition,
Wärmeverluste und Leistungsbegrenzung können die Dauer verändern. Der späteste
Start ist Intervallende minus geschätzte Heizdauer. Bei gültiger Netz-Messung
und aktiver Viertelstundenüberwachung wird vor diesem Zeitpunkt gewartet, wenn
die geschätzte freie Netzleistung unter der Heizstableistung liegt. Am
spätesten Start wird Leistung angefordert. PV-Überschussautomatik,
Legionellenprogramm und manuelles Heizen bleiben eigenständige Betriebsarten.

Während des gesamten aktiven Tarifintervalls läuft die Energieabfrage im
Normalintervall, auch wenn die nächtliche Abfragepause konfiguriert ist. Bei
Unterschreiten der Notfallgrenze außerhalb des Intervalls wird ebenfalls
weiter gemessen, sofern die Viertelstundenbegrenzung aktiviert ist. Für alle
Heizarten gilt dieselbe Leistungsgrenze. Ohne frische Netzmessung ist ihre
sichere Berechnung unmöglich; bei eingeschalteter Begrenzung bleibt die
Heizleistung deshalb null, auch im Notfall. Die GUI zeigt diesen Zustand an.

Die Vue-Seiten `/heizplan`, `/netzbezug` und `/netzbezug/verlauf` trennen
Einstellungen, Livewerte und Historie. `/netzbezug/verlauf` verwendet die
bestehende Dexie-IndexedDB im jeweiligen Browser. Abgeschlossene Intervalle
werden anhand ihres UTC-Starts dedupliziert und mit Messabdeckung,
Vollständigkeit, Bezug und damaligem Limit gespeichert. Ein Durchschnitt wird
nur bei vollständigen Messungen angezeigt. CSV-Export und Löschen sind lokal.
Der Lösch-Cursor bleibt erhalten, damit der zuletzt wiederholt gesendete
WebSocket-Wert nicht sofort erneut erscheint. Der ESP liefert aktuell nur das
letzte abgeschlossene Intervall: Wenn der Browser geschlossen oder die
Anwendung nicht angemeldet ist, fehlen ältere Intervalle dauerhaft in dieser
lokalen Historie. Es handelt sich um
Schätzwerte, nicht um abrechnungsfähige Zählerdaten.
