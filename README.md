# Content

Es soll ein Produkt (HW, SW) geschaffen werden, welches analog dem [Ohmpilot von Fronius](https://www.fronius.com/de/solarenergie/installateure-partner/technische-daten/alle-produkte/l%C3%B6sungen/fronius-w%C3%A4rmel%C3%B6sung/fronius-ohmpilot/fronius-ohmpilot) funktioniert. Damit soll überschüssiger PV-Strom hausintern verbraucht werden (z.B. Warmwasseraufbereitung). Als Steuerzentrale wird ein uController - [esp32 mit kleinem Display](https://www.lilygo.cc/products/lilygo%C2%AE-ttgo-t-display-1-14-inch-lcd-esp32-control-board) - herangezogen. Dieser ESP ist mit WLAN und Bluetooth ausgestattet. Als ReferenzWR werden Fronius-WR herangezogen, wobei hier der Modbuszugang freizuschalten ist. 

## Installation /InBetriebnahme / Fehler
Unter [https://github.com/htlWels/energyJunkies/blob/main/projects/sware/ohmPilotFronius/doc/install.md] ist eine Installationsanleitung zu finden, welche die einzelnen Schritte für eine erfolgreiche Installation Schritt für Schritt erläutert.
Es können natürlich noch Fehler oder seltsames Verhalten auftreten. Diese könnten unter [https://github.com/htlWels/energyJunkies/blob/main/projects/sware/ohmPilotFronius/doc/error.md] dokumentiert werden und werden dann behoben. 

## Anforderungen

Das Produkt wird in einen hardware- und softwareabhängigen Teil aufgespaltet. Es ist ein "Hobby"-Projekt, aber dennoch sollten die wesentlichen Anforderungen erfasst werden. Prinzipiell wird davon ausgegangen, dass die zu betreibenden Heiz/Lade -elemente einen 3-phasigen Anschluß haben, die dann einzeln angesteuert werden.

### HW

Der OhmPilot von Fronius kann dzt. um die gut € 1.000,-- erworben werden, d.h. die Entwicklungskosten für die HW sollten deutlich unterschritten werden (!)

* Die GPIOs des ESP32 werden mit 3.3 V betrieben. Über eine entsprechende TransistorSchaltung kann je Phase dann per Relais durchgeschaltet werden. Alternativ könnte ein Pegelwandler 3.3V auf 5V genutzt werdenererer
* 2 Temperatursensoren sorgen für einen "Not-Aus", d.h. wenn beispielsweise das Wasser schon 85° hat, darf nicht mehr geheizt werden. Sicherheitshalber werden 2 Sensoren verwendet für den Fall, dass einer ausfällt. Die gesteuerte Last könnte auch indirekt über ein Sicherheitsthermostat indirekt angeschlossen werden.
* Lage des Displays (vertikal | horizontal) ?
* Vorschläge von Ej - Blockschaltbilder liegen in **projects/ohmPilotFronius/**
* EJ-EMV Filter (Elektromagnetischer Filtervorsatz, auf diesen wird in der ersten Phase verzichtet bzw. Vorversuche gestartet)
-  EJ-Control Board (ESP32 TTGO, Eingabe, Ausgabe Schnittstellen, Versorgt über die Power Unit)
-  EJ-Power Unit1 (Phasenanschnitt mit Modul 2-10V vom Armin bzw. Modul 0-20mA vom Oliver, sehr große EMV Probleme, erste Hardware für Versuche)
-  EJ-Power Unit2 (PWM Endstufe, eventuell Schülerprojekt 23/24, große EMV Probleme)
-  EJ-Power Unit3 (Pulspakete, eventuell Schülerprojekt 23/24, wenig EMV Probleme, AMIS Zähler?)
-  EJ-Power Unit4 (eine Art 7 Takt Schaltung wie bei den alten E-Herd Platten, sehr wenig EMV Probleme, keine stufenlose Regelung)

#### Verbesserungsvorschläge
- Rote LED, welche eine Warnung/Fehler anzeigt; dieser wird zusätzlich am Display angezeigt
- die dzt. 4 Taster sollten mit einer Brücke einstellbar sein ; somit stehen 4 GPIOs zur Verfügung, die anderweitig eingesetzt werden können (potentialfreie Ausgänge, über Buchse ausführen
- PWM Verbindung herstellen (bei neuer Platine)
- 4 Schalter abänderung und verwenden für
  * S1: Zustands-/Fehler LED
  * S2: Relais ausführen
  * S3/S4: eventuell die anderen 2 auch als potentialfreien Ausgang ausführen (Relais)
- Shelly Einbindung (reine SW)

### SW

* Setup: Folgende Parameter sind benutzer- bzw. analagenabhängig:
  - Einstellung der WLAN-Zugangsdaten (SSID,Password)
  - IP-Adresse Modbus
  - Heizstableistung (für die rechnerische Festlegung der in das Heizsystem geleiteten Energie)
  - Einspeisebeschränkung (wieviel kW dürfen eingespeist werden)
 -  <del>PI-Regler für den 0-100 % Ausgang (Proportional(P),Integral(I),Differenzial(D)) - Default: P=1, I=0.5, D=0 (FLoat)</del>
  - <del>Regelbereich Hysterese für die PWM-Steuerung (z.B. 5.900, 5.800 - Hysterese: 100) - oder von Einspeisebeschränkung 100 Watt abziehen und Hysteresewert </del>
  - Für die Eingabe ist eine WebSite zu verwenden, wobei der uController (ESP32) bei der Erst-Inbetriebnahme oder Wechsel des Wlan-Einwählpunktes als AccessPoint fungiert. Die notwendigen Informationen (SSID,IP-Adresse) sind per Display anzuzeigen.
  - Die Setup-Basisdaten sind per EEProm zu sichern, die bei einem Restart dann herangezogen werden können. 
* Auf dem Display sollten die wichtigsten Informationen angezeigt werden können:
  - aktive Netzwerkverbindung
  - IP-Adresse
  - aktuelle Produktion, davon Überschuss
  - Zuteilung an die 3 Phasen
  - aktuelle Temperatur des Wasses, gemessen durch die 2 TempSensoren
  - Fehler
  - ?
* Die Kommunikation wird per WLAN durchgeführt, wobei regelmäßig zu prüfen ist, ob die WLAN-Verbindung noch funktioniert
  - ?? hat nur tagsüber Sinn
  - was passiert, wenn die Kommunikation nicht mehr funktioniert - Fehlermeldung am Display ( ! ), Abschalten des Reglers für den Heizstab
- Der WR wird per Modbus (TCP) angesteuert & setzt eine funktionierende WLAN-Verbindung voraus.
- Alternativ kann auch der AmisReader verwendet werden, wobei hier weniger Informationen zur Verfügung stehen. 
- Da Fronius bei der kostenlosen Variante die Verbrauchsdaten nur für 3 Tage loggt, kann der ESP32 diese mitloggen. Dafür ist jedoch ein CardReader notwendig.
- kleines REST-API, wo die wichtigsten Daten per Web verfügbar sind, die IP-Adresse steht ja auf dem DIsplay.
- Benachrichtigung per Email, dass irgend etwas passiert ist ( ? )
- Der Regler muss bei Erreichen des Temperaturlimits (per Setup-Dialog einstellbar) sämtliche Kanäle abschalten.
- Sofern ein Akku vorhanden ist, soll die Priorisierung einstellbar sein (d.h. wird zuerst der Boiler oder der Akku geladen).

## Offene Fragen
* Initialisierung/sETUP: Wenn Netzwerk nicht funktioniert, kommt man automatisch in den AccessPoint-Modus;
* ```Wenn beim Setup kein gültiges WLAN-Netzwerk vorgefunden wird, bootet der ESP32 im AP-Modus mit der Default-Adresse 192.168.4.1 und bietet dazu auch einen offenen Zugang namens "EnergyJunkies" an.```
* Was macht man bei auftretenden Fehlern?
* ```Diese werden sowohl am Display als auch am Web-Interface angezeigt.```

## Was einem sonst noch so einfällt

* Bezüglich Tag/Nacht-Betrieb: Prinzipiell macht die Steuerung nur dann Sinn, wenn Strom produziert wird -also tagsüber. Wahrscheinlich werden künftig unterschiedliche h-Strompreise angeboten werden, d.h. bei schönem Wetter ist mehr Solarstrom da und daher günstiger ( ? ) oder der Strom ist dann teuerer, wenn mehr gebraucht wird (z.B. vormittags). Einen Boiler muss man immer betreiben und hier könnte es Sinn machen, auch einen Nachtbetrieb einzuführen, da hier der Strom wahrscheilich günstiger ist.
* Es wäre für die Optimierung der Energiekosten (welcher Tarif ist für das Aufheizen/Aufladen am günstigsten) günstig, wenn man den Wetterbericht kennt, denn dann weiß man ja, ob genug Solarstrom vorhanden ist, den Task durchzuführen (z.B. Aufladen).
* Künftig werden wahrscheinlich variable Einspeistarife vorgegeben werden, variable Stromkosten für den Einkauf gibt es ja schon. Hier kann dann optimiert werden in Abstimmung mit einem eventuell vorhandenen Akku, wann wer wie geladen / in das Netz gespeist wird.
* Poolpumpe bzw. Poolwärmepumpe: Poolpumpe wälzt das Wasser im Becken um, egal welche Qualität das Wasser hat. Hier könnte in Verbindung mit einem Frequenzumformer die Leistung der Pumpe reduziert werden, wenn man weiß, welche Qualität das Wasser dzt. hat. Die Wärmepumpe kann ebenfalls in Abhängigkeit von Überstrom gesteuert werden (Ein- bzw. Ausschalten per TCP?)
* Setup: Jeder hat ja seine eigen Zugangsdaten - wie sollen diese am Besten berücksichtigt werden. Der geringste Aufwand ist die Erstellung eines eigenen setup.h-Files, in der sämtliche spezifische Daten eingetragen werden. Elegant wäre natürlich das Setup per Handy, wobei der ESP im Setup-Modus bootet und einen eigenen AP zur Verfügung stellt, in der sich eine App einwählt und dann die Zugangsdaten einstellt. Diese Daten werden dann ins Eprom verfrachtet - ``Ẁird so durchgeführt``
* Im Prototyp sind ja 4 zusätzliche Taster vorgesehen - es ist hier zu definieren, welche Funktionalität hier hinterlegt wird.
* Thema WLAN/Setup: ich hab bereits ein Projekt mit einem ESP2866 am Start und genau diese Problematik. Ich hab 2 Bibliotheken im Einsatz: 
  - [WIFI-Manager](https://github.com/tzapu/WiFiManager) Hotspot + Webserver für WLAN Konfiguration + eigenes Setup möglich (ums Speichern des eigenen Setups muss man sich selbst kümmern)
  * [WebConfig](https://github.com/GerLech/WebConfig)eigener Webserver ausschließlich für Konfiguration, Daten werden am FS gespeichert
* HomeKit Integration: ich hab bereits etliche Geräte mit Apple HomeKit im Einsatz und auch schon eigene Geräte HomeKit tauglich gemacht. Meine Idee wäre, einfach einen Schalter mit einem ESP zu simulieren (z.B. Stromüberproduktion-> Schalter ist ON, ...) Dieser Schalter lässt sich easy in die bestehende Infrastruktur einbinden und dank Homekit können dann eigene Abläufe konfiguriert werden -> "Schalter Stromüberproduktion" = ON -> schalte über einen Shelly (ebenfalls mit Homekit Firmware geflasht) den Warmwasserboiler ein.

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
