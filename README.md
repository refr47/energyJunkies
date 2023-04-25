# Content
Es soll ein Produkt (HW, SW) geschaffen werden, welches analog dem [Ohmpilot von Fronius](https://www.fronius.com/de/solarenergie/installateure-partner/technische-daten/alle-produkte/l%C3%B6sungen/fronius-w%C3%A4rmel%C3%B6sung/fronius-ohmpilot/fronius-ohmpilot) funktioniert. Damit soll überschüssiger PV-Strom hausintern verbraucht werden (z.B. Warmwasseraufbereitung). Als Steuerzentrale wird ein uController - [esp32 mit kleinem Display](https://www.lilygo.cc/products/lilygo%C2%AE-ttgo-t-display-1-14-inch-lcd-esp32-control-board) - herangezogen. Dieser ESP ist mit WLAN und Bluetooth ausgestattet. Als ReferenzWR werden Fronius-WR herangezogen, wobei hier der Modbuszugang freizuschalten ist. 
## Anforderungen

Das Produkt wird in einen hardware- und softwareabhängigen Teil aufgespaltet. Es ist ein "Hobby"-Projekt, aber dennoch sollten die wesentlichen Anforderungen erfasst werden. Prinzipiell wird davon ausgegangen, dass die zu betreibenden Heizelemente einen 3-phasigen Anschluß haben, die dann einzeln angesteuert werden. 
### HW
Der OhmPilot von Fronius kann dzt. um die gut € 1.000,-- erworben werden, d.h. die Entwicklungskosten für die HW sollten deutlich unterschritten werden (!)
- Die GPIOs des ESP32 werden mit 3.3 V betrieben. Über eine entsprechende TransistorSchaltung kann je Phase dann per Relais durchgeschaltet werden. 
- Ein Spannungsregler [6-12 V] sorgt für die Versorgung des ESP32 und ein Netzteil für die Stromversorgung (z.B. 12 V).
- 2 Temperatursensoren sorgen für einen "Not-Aus", d.h. wenn beispielsweise das Wasser schon 85° hat, darf nicht mehr geheizt werden. Sicherheitshalber werden 2 Sensoren verwendet für den Fall, dass einer ausfällt.
- RTC ( ? ) für Tag/Nacht unterscheidung, wo es eventuell sinnlos ist, irgend etwas abzufragen - siehe "Offene Fragen"

### SW
- Auf dem Display sollten die wichtigsten Informationen angezeigt werden können:
    - aktive Netzwerkverbindung
    - IP-Adresse
    - aktuelle Produktion, davon Überschuss
    - Zuteilung an die 3 Phasen
    - aktuelle Temperatur des Wasses, gemessen durch die 2 TempSensoren
    - Fehler
 - Die Kommunikation wird per WLAN durchgeführt, wobei regelmäßig zu prüfen ist, ob die WLAN-Verbindung noch funktioniert 
    - ?? hat nur tagsüber Sinn - RTC oder so was ähnliches
    - was passiert, wenn die Kommunikation nicht mehr funktioniert - Fehlermeldung am Display ( ! )
 - Der WR wird per Modbus (TCP) angesteuert & setzt eine funktionierende WLAN-Verbindung voraus. 
 - Da Fronius bei der kostenlosen Variante die Verbrauchsdaten nur für 3 Tage loggt, kann der ESP32 diese mitloggen. Dafür ist jedoch ein CardReader notwendig.
 - kleines REST-API, wo die wichtigsten Daten per Web verfügbar sind, die IP-Adresse steht ja auf dem DIsplay.
 - Benachrichtigung per Email, dass irgend etwas passiert ist ( ? ) 

 Eventuell sollen zusätzliche Agenden übernommen werden. 

# Offene Fragen
- Bezüglich Tag/Nacht-Betrieb: Prinzipiell macht die Steuerung nur dann Sinn, wenn Strom produziert wird -also tagsüber. Wahrscheinlich werden künftig unterschiedliche h-Strompreise angeboten werden, d.h. bei schönem Wetter ist mehr Solarstrom da und daher günstiger ( ? ) oder der Strom ist dann teuerer, wenn mehr gebraucht wird (z.B. vormittags). Einen Boiler muss man immer betreiben und hier könnte es Sinn machen, auch einen Nachtbetrieb einzuführen, da hier der Strom wahrscheilich günstiger ist. 
- Setup: Jeder hat ja seine eigen Zugangsdaten - wie sollen diese am Besten berücksichtigt werden. Der geringste Aufwand ist die Erstellung eines eigenen setup.h-Files, in der sämtliche spezifische Daten eingetragen werden. Elegant wäre natürlich das Setup per Handy, wobei der ESP im Setup-Modus bootet und einen eigenen AP zur Verfügung stellt, in der sich eine App einwählt und dann die Zugangsdaten einstellt. 
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
