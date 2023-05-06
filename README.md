# Content

Es soll ein Produkt (HW, SW) geschaffen werden, welches analog dem [Ohmpilot von Fronius](https://www.fronius.com/de/solarenergie/installateure-partner/technische-daten/alle-produkte/l%C3%B6sungen/fronius-w%C3%A4rmel%C3%B6sung/fronius-ohmpilot/fronius-ohmpilot) funktioniert. Damit soll überschüssiger PV-Strom hausintern verbraucht werden (z.B. Warmwasseraufbereitung). Als Steuerzentrale wird ein uController - [esp32 mit kleinem Display](https://www.lilygo.cc/products/lilygo%C2%AE-ttgo-t-display-1-14-inch-lcd-esp32-control-board) - herangezogen. Dieser ESP ist mit WLAN und Bluetooth ausgestattet. Als ReferenzWR werden Fronius-WR herangezogen, wobei hier der Modbuszugang freizuschalten ist.

## Anforderungen

Das Produkt wird in einen hardware- und softwareabhängigen Teil aufgespaltet. Es ist ein "Hobby"-Projekt, aber dennoch sollten die wesentlichen Anforderungen erfasst werden. Prinzipiell wird davon ausgegangen, dass die zu betreibenden Heiz/Lade -elemente einen 3-phasigen Anschluß haben, die dann einzeln angesteuert werden.

### HW

Der OhmPilot von Fronius kann dzt. um die gut € 1.000,-- erworben werden, d.h. die Entwicklungskosten für die HW sollten deutlich unterschritten werden (!)

- Die GPIOs des ESP32 werden mit 3.3 V betrieben. Über eine entsprechende TransistorSchaltung kann je Phase dann per Relais durchgeschaltet werden. Alternativ könnte ein Pegelwandler 3.3V auf 5V genutzt werdenererer
- 2 Temperatursensoren sorgen für einen "Not-Aus", d.h. wenn beispielsweise das Wasser schon 85° hat, darf nicht mehr geheizt werden. Sicherheitshalber werden 2 Sensoren verwendet für den Fall, dass einer ausfällt. Die gesteuerte Last könnte auch indirekt über ein Sicherheitsthermostat indirekt angeschlossen werden.
- Lage des Displays (vertikal | horizontal) ?
- Vorschläge von Ej (Blockschaltbild)[file:///projects/ohmPilotFronius/]
- 1. EJ-EMV Filter (Elektromagnetischer Filtervorsatz, auf diesen wird in der ersten Phase verzichtet bzw. Vorversuche gestartet)
- 2. EJ-Control Board (ESP32 TTGO, Eingabe, Ausgabe Schnittstellen, Versorgt über die Power Unit)
- 3. EJ-Power Unit1 (Phasenanschnitt mit Modul 2-10V vom Armin bzw. Modul 0-20mA vom Oliver, sehr große EMV Probleme, erste Hardware für Versuche)
- 3. EJ-Power Unit2 (PWM Endstufe, eventuell Schülerprojekt 23/24, große EMV Probleme)
- 3. EJ-Power Unit3 (Pulspakete, eventuell Schülerprojekt 23/24, wenig EMV Probleme, AMIS Zähler?)
- 3. EJ-Power Unit4 (eine Art 7 Takt Schaltung wie bei den alten E-Herd Platten, sehr wenig EMV Probleme, keine stufenlose Regelung)

### SW

- Auf dem Display sollten die wichtigsten Informationen angezeigt werden können:
  - aktive Netzwerkverbindung
  - IP-Adresse
  - aktuelle Produktion, davon Überschuss
  - Zuteilung an die 3 Phasen
  - aktuelle Temperatur des Wasses, gemessen durch die 2 TempSensoren
  - Fehler
  - ?
- Die Kommunikation wird per WLAN durchgeführt, wobei regelmäßig zu prüfen ist, ob die WLAN-Verbindung noch funktioniert
  - ?? hat nur tagsüber Sinn
  - was passiert, wenn die Kommunikation nicht mehr funktioniert - Fehlermeldung am Display ( ! )
- Der WR wird per Modbus (TCP) angesteuert & setzt eine funktionierende WLAN-Verbindung voraus.
- Da Fronius bei der kostenlosen Variante die Verbrauchsdaten nur für 3 Tage loggt, kann der ESP32 diese mitloggen. Dafür ist jedoch ein CardReader notwendig.
- kleines REST-API, wo die wichtigsten Daten per Web verfügbar sind, die IP-Adresse steht ja auf dem DIsplay.
- Benachrichtigung per Email, dass irgend etwas passiert ist ( ? )
-

Eventuell sollen zusätzliche Agenden übernommen werden.

# Offene Fragen

- Bezüglich Tag/Nacht-Betrieb: Prinzipiell macht die Steuerung nur dann Sinn, wenn Strom produziert wird -also tagsüber. Wahrscheinlich werden künftig unterschiedliche h-Strompreise angeboten werden, d.h. bei schönem Wetter ist mehr Solarstrom da und daher günstiger ( ? ) oder der Strom ist dann teuerer, wenn mehr gebraucht wird (z.B. vormittags). Einen Boiler muss man immer betreiben und hier könnte es Sinn machen, auch einen Nachtbetrieb einzuführen, da hier der Strom wahrscheilich günstiger ist.
- Es wäre für die Optimierung der Energiekosten (welcher Tarif ist für das Aufheizen/Aufladen am günstigsten) günstig, wenn man den Wetterbericht kennt, denn dann weiß man ja, ob genug Solarstrom vorhanden ist, den Task durchzuführen (z.B. Aufladen).
- Künftig werden wahrscheinlich variable Einspeistarife vorgegeben werden, variable Stromkosten für den Einkauf gibt es ja schon. Hier kann dann optimiert werden in Abstimmung mit einem eventuell vorhandenen Akku, wann wer wie geladen / in das Netz gespeist wird.
- Poolpumpe bzw. Poolwärmepumpe: Poolpumpe wälzt das Wasser im Becken um, egal welche Qualität das Wasser hat. Hier könnte in Verbindung mit einem Frequenzumformer die Leistung der Pumpe reduziert werden, wenn man weiß, welche Qualität das Wasser dzt. hat. Die Wärmepumpe kann ebenfalls in Abhängigkeit von Überstrom gesteuert werden (Ein- bzw. Ausschalten per TCP?)
- Setup: Jeder hat ja seine eigen Zugangsdaten - wie sollen diese am Besten berücksichtigt werden. Der geringste Aufwand ist die Erstellung eines eigenen setup.h-Files, in der sämtliche spezifische Daten eingetragen werden. Elegant wäre natürlich das Setup per Handy, wobei der ESP im Setup-Modus bootet und einen eigenen AP zur Verfügung stellt, in der sich eine App einwählt und dann die Zugangsdaten einstellt. Diese Daten werden dann ins Eprom verfrachtet ?
- Im Prototyp sind ja 4 zusätzliche Taster vorgesehen - es ist hier zu definieren, welche Funktionalität hier hinterlegt wird.

# IDE/PlatformIO

<ol>
    <li>[C/C++ COmpiler mit Platformio](https://alsaibie.github.io/embedded_ccpp/ide_setups/pio_for_host/)
    </li>
</ol>
## Links & Foren
<ol>
<li>
Photovoltaik Form [Photovoltaik Form](https://www.photovoltaikforum.com/thread/50214-gen24-modbus-tcp/)
</li>
    <li>
        [TTGO ESP] (https://www.lilygo.cc/products/lilygo%C2%AE-ttgo-t-display-1-14-inch-lcd-esp32-control-board)
    </li>
    <li>
        Mark Down Formatierung (https://www.markdownguide.org/cheat-sheet/#extended-syntax)
    </li>

</ol>
