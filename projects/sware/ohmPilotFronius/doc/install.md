# Content

Anleitung für das Flashen des Projekts (esp32) mit Hilfe von PlatformIO

## Versionen
v0.9  Erste Version

## Fehler
Auftretende Fehler oder seltsames Verhalten soll unter "Fehler" - wie im Readme beschrieben - dokumentiert werden. Dazu ist die Angabe der Version notwendig.

## Voraussetzungen /  PlatformIO als IDE / VsCode / PlugIns
- PlatformIO
Das Plugin für VSCode sollte vorinstalliert sein und funktioniert an und für sich ganz gut. Problematisch ist, dass es ab und zu relativ selbstständig irgend etwas unternimmt und dies in einer unverständlichen Fehlermeldung endet.
- C/C++ Plugin
- Beautifier für Syntaxhilighting
- esp-idf 
- git

Das Projekt via Github clonen; PlattformIO wird die gesamte Toolchain des esp32 installieren; zusätzlich werden die Abhängigkeiten (Libs) aufgelöst und heruntergeladen. Dies kann - je nach Internetverbindung - schon eine Zeit dauern.

## Implementierung
Der ESP32 läuft prinzipiell auf RTOS und kann auch in diesem Modus programmiert werden (https://tutoduino.fr/en/discover-freertos-on-an-esp32-with-platformio/). Die Anzahl der verfügbaren Bibliotheken ist überschaubar, daher werden die meisten Projekte im "Arduino"-Modus betrieben. Man kann dann alle für Arduino geschriebenen Bibliotheken verwenden und auch die Programmstruktur ist analog aufgebaut (setup() und loop()). Da jeder Arduino-Bibliotheken erzeugen und veröffentlichen kann, gibt es keine Qualitätskontrolle. Prinzipiell kann C bzw. C++ eingesetzt werden, wobei aber bei bestimmten Objekten (z.B. String) Vorsicht geboten ist, da die dynamische Speicherverwaltung teilweise Probleme aufwirft, die dann zum Absturz des Programmes führen und es zu einem Neustart kommt. Im ungünstigen Fall kommt es zu einer Loop (Fehler, booten). Die Bibliotheken weisen teilweise eine geringe Qualität auf und können aufgrund auftretender Seiteneffekte zu Problemen führen. Das Hauptproblem ist jedoch die schleißige Toolchain, sodass beispielsweise ein Debugging fast nicht möglich ist. Die JTAG-Untersützung wird zwar in der Doku erwähnt, aber wie man dann tatsächlich einen JTAG-Debugger anschließt, ist ein gut gehütetes Geheimnis.  Dazu kommt die Vielfalt an esp32-Modellen mit teilweise falschen PIN-Belegungen und einer Minimal-DOkumentation. Grundsätzlich ist der ESP32 ein reichlich ausgestatteter µController, der am besten mit RTOS betrieben wird - aber dann fallen alle Arduino-Libs weg. 

Problematisch ist auch die Programmiersprache C mit der nicht vorhandenen Speicherverwaltung. Da es quasi keinen Debugger gibt, sucht man oft stundenlang einen Fehler, da nur der "Poor Man Debuger" in Form von printf zur Verfügung steht. 

## Display Configuration

Das verwendete Display (lilygo-s3) wird über den SPI-Bus angesteuert und die entsprechende Bibliothek muss dafür konfiguriert werden. Ansonst bleibt es dunkel.
Der Config-File **_User_Setup_Select.h_** befindet sich im TFT*eSPI-Verzeichnis und hat hat innerhalb der Direktive \_USER_SETUP_LOADED* nur einen Eintrag in der Nähe von Zeile 134: _#include <User_Setups/Setup206_LilyGo_T_Display_S3.h> // For the LilyGo T-Display S3 based ESP32S3 with ST7789 170 x 320 TFT;_ ansonst ist alles auskommentiert.

[User_Setup_Select.h](./tft/User_Setup_Select.h)

## Verschiedene Flash-Bereiche

Der ESP32 stellt intern ein FlashFilesystem (SPIFF) zur Verfügung, welche vom FlashSpeicher für das Programmimage getrennt ist und von diesem quasi "gemounted" werden kann. Das SPIFF-System wird in diesem Projekt für die Speicherung des WebClients (also der html-files und allen dazugehörigen Ressourcen) verwendet. Diese liegen im Projekt im Ordner "data".

Dementsprechend gibt es 2 verschiedene Flashvorgänge.

- a) Flashen des SPIFF-Bereichs
- b) Flashen des Programm Images

Beides sind separate Vorgänge. Wenn am Client keine Änderungen sind, braucht dieser Bereich auch nicht reflasht werden, sondern es genügt ein einmaliger Flashvorgang.

ad a) Vorgehensweise

- In der linken Menüzeile Symbol für PlatformIO suchen
- Anklicken und im oberen Bereichh (Project Task) den lilygo suchen und anklicken
- Es wird eine Liste angezeigt und hier den Build Filesystem Image und Upload Filesystem Image" anklicken
  **Hinweis** Sowohl bei "Erase Flash" als auch beim Flashen des SPIFF-Bereichs darf keine Monitor/serielle Verbindung zum ESP offen sein, da sich beide sperren. Die offenen Tasks werden rechts unten in einem kleinen Kasten angezeigt und die serielle Verbindung wird als "Monitor" bezeichnet. Hier einfach mit der Maus darüber fahren und den dann angezeigten Mülleimer anklicken.

ad b) Flashen des Programm-Images
Das Programm muss fehlerfrei übersetzen (kleines Häckchen) und kann dann ohne weitere Maßnahmen mit dem Transfersymbol (kleiner Pfeil nach rechts) geflasht werden. Dies kann beliebig oft wiederholt werden, das SPIFF-System ist davon nicht betroffen.

## Einstellen diversers Flags in ***platformio.ini***

Diese Initialisierung-/Configdatei ist zentraler Bestandteil für das Laden von Bibliotheken und dem Compile-Vorgang, der u.a. durch diverse ***Defines*** gesteuert wird (bedingtes Compile). Im Sourcecode ist dann oftmals die ***ifdef*** Anweisung zu sehen, dass bestimmte Teile vom COmpile-Vorgang ein- bzw. ausschließt. Die wichtigsten Flags wären:

Hinweis: eine nachgestellte 1 deaktiviert das Define und hat zur Folge,dass der betroffene Bereich nicht compiliert wird. 

DUSE_ESP_IDF_LOG -DCORE_DEBUG_LEVEL=5           # LOG Filter

- DLOGFILE_SYS='"/logSYS.txt"'                    # Name des Logfiles am CardREader
- DLOGFILE_INVERTER='"/logInv.txt"'               # Ausgabe für Inverter Daten (z.B. Produktion,...)
- DTAG='"EJunkies"'                               # Tag für ESP Logging System
- DEJ=1                                           # TestProg für EJ (Schalter) am Board;
- DWEB=1 # Zusätzliche FUnktionalität für REST
- DCORS_DEBUG=1   # Debug der REST
- DTEST_PID_WWWW1=1  # Test des Reglers, wobei dann die in der RestSchnittstelle unter Stammdaten eingegeben Daten nach dem Speichern übernommen werden
- DMQTT='"10.0.0.2"'   # falls man einen mqtt-Server nutzt
- -DMODBUS_VERBOSE=1 	# Modbus Kommunikation im Debug-Modus

  -DFRONIUS_IV=1		# Bei eineme Fronius WR: Verwendung des Solar API (http Protokoll)
- DWEATHER_API='"https://api.open-meteo.com"'
- -DINFLUX='"http://rantanplan-ethernet:8086"'
- -DAMIS_READER_DEV=1 # Amis Reader Support; wenn Fronius und Amis-Reader aktiviert ist, hat Fronius den Vorzug; Wenn kein Fronius IV eingesetzt wird, hat man wesentlich weniger Daten zur Verfügung, da der Amis-Reader nur den Im-/Export zur Verfügung stellt.
- -DSHELLY=1  # Shelly Geräte einbinden 

***Hinweis***: Das Define ist deaktiviert, wenn es einen anderen Namen hat, z.B. -DEJ1; platformIO sorgt sich dann um den Rest im Sourcecode


## Herunterladen per GIT
Das VSCode-Plugin ist relativ genau und funktioniert ohne Probleme. Für das erstmalige Herunterladen empfiehlt sich die console-basierte Methode per ````git clone https://github.com/htlWels/energyJunkies.git````
Sodann kann VSCode gestartet werden im ````energiejunkies````-Ordner. Hier kann es passieren, dass PlatformIO die Initialisierungsdatei nicht auf Anhieb findet. Es gibt hier den Button "Pick Folder" und dieser öffnet einen FileDialog. Dann sucht man unter ````projects/sware/```` den Ordner ````ohmPilotFronius````. Bei der Erstinstallation dauert es - je nach Internetanbindung - schon eine Zeit, bis PlatformIO fertig ist. PlatformIO ist via Python realisiert und braucht daher auch Ressourcen in Form von RAM. 



## Compile/Flash

Zuerst muss das Projekt fehlerfrei übersetzt (kleines Häckchen in der unteren Menüleiste, neben dem kleinen Häuschen) werden können und ein Image für den esp32 erzeugt werden. Es kommen noch einige Warnings, aber die sind eher bedeutungslos und kommen größtenteils von den verwendeten Libs.

Nach dem Übersetzen kann nun mit dem Flash begonnen werden. Dazu ist folgendes zu beachten

- Der Energie-Harvester ist mit einer REST-Schnittstelle ausgestattet, wobei der Client im "data"-Ordner liegt. Dieser Ordner enthält alle notwendigen Ressourcen für die WWW-Anbindung und wird vom Client (Browser) geladen. Die REST-Schnittstelle steht immer zur Verfügung und MUSS extra geflasht werden.
- Dazu muss auf der linken Seite im VS-Code Editor das PlattformIO Symbol ausgewählt werden und dann in den "Project Tasks" das Projekt "lilygo-t-display-s3" ausgewählt werden. Unter "Plattform" sieht man dann die Menüpunkte "Build Filesystem Image" und "Upload Filesystem Image".
- Zuerst ist das FilesystemImage zu erstellen und dann zu flashen.
- Für die Ablage dieser Ressourcen wird das vom ESP-IF zur Verfügung gestellte Filesystem (SPIFFs) verwendet. Daher auch die Aufspaltung in 2 Flash-Vorgänge.
- Sodann kann das compilierte Image geflasht werden und der esp32 startet.

## Inbetriebnahme

Der E-Harvester speichert sich die Konfigurationsdaten im Flash-Speicher und die REST-Schnittstelle bedient sich dieser Daten und speichert dort diese auch wieder ab.

Es gibt da Stammdaten und Mobilitätsdaten und die Logfiles

- Stammdaten: Alles, was man für den Betrieb so braucht (ESSID, PID-Einstellungen, ....)
- Mobilitätsdaten: Ein Teil der Informationen wird über das kleine Display ausgegeben, die meisten werden per WWW zur Verfügung gestellt. Es sind Life-daten (die Daten werden per "WebSockets" aktualisiert)
- Logfiles: Es wird einiges mitprotokolliert und auf dem CardReader gespeichert. Diese Logfiles können via Browser heruntergeladen werden.

### Access-Point Modus

- Falls sich der ESP nicht in das im Setup hinterlegte WLAN einloggen kann, geht er in den AP-Modus und stellt ein eigenes WLAN zur Verfügung; dieses wird auch am Display angezeigt und ist frei (d.h. kein Benuter/Password).
  - Default-Adresse: **192.168.4.1**
  - Default ESSID: **Energy Junkies**
- Nun kann man sich in dieses WLAN einwählen und obige IP-Adresse per Browser aufrufen. Man gelangt dann zu einem Login-Dialog, wobei hier der Fallback-Zugang mit **admin** und Password **password** hinterlegt ist.
- Nach dem Speichern wird der ESP32 kommentarlos neu gestartet.

### Client Modus

- Wenn die hinterlegten Daten stimmen, kann sich der ESP32 in das WLAN einwählen und läuft einen Initialisierungsprozess durch, indem sämtliche Komponenten geprüft werden, ob diese vorhanden sind und funktionieren. Dies sind
  - Netzwerk
  - SPIFF-Filesystem
  - WebServer
  - NTP (Zeitserver)
  - CARD-Reader
  - Logging-Ressourcen (bedingt gültigen CARD-Reader)
  - Temperatur-Sensorik
  - Modbus

* Sämtliche Schritte werden am Display angezeigt und bei einem Fehler wird dies rot gekenntzeichnet.

## WebInterface

Die Kommunikation des Clients mit dem Server wird per

- Ajax (Stammdaten) und
- WebSockets (Aktuelle Daten)
  durchgeführt.

### Stammdaten
Wie schon erwähnt, wird die gesamte Konfiguration per WebInterface durchgeführt, wobei die Konfigurationsdaten wiederum in einem separaten Flashbereich abgespeichert werden. Es werden keine gesonderten Anforderungen an die Firewall gestellt, sämtlicher Traffic läuft - zumindest dzt - noch über Port 80. Die einzelnen Parameter sind selbst erklärend und müssen nach dem Updaten an den Server gesendet werden. Einige Parameter unterstützen ein "HotUpdate" und werden im laufenden Betrieb übernommen, andere wie beispielsweise Netzwerk-SSID bzw. Password führen zu einem Neustart.

## Aktuelle Daten
Je nach verfügbarem DatenInterface (Fronius oder AMIS-Reader, Akku) werden unterschiedliche Bewegungsdaten angezeigt bzw. auch aktualisiert. 

## Verhalten/Reconnect
Da relativ viel Kommunikation anfällt, wird immer überprüft, ob eine WLAN-Verbindung noch valide ist und es wird gegebenenfalls ein Reconnect veranlasst. Sobald eine fehlerhafte Netzwerkverbindung auftritt, wird der Regler ausgeschaltet, da ja keine vernünftigen Daten mehr vorliegen. 

## Logfile
Es wird in der aktuellen Version von VSCode immer ein Logfile angelegt. Dies kann über die ````plattform.ini````gesteuert werden. 




***Hinweise***

- Bei der Erst-Inbetriebnahme wird das Flash mit Standard-Werten beschrieben. Das Programm überprüft zu diesem Zweck, ob es gültige Einträge findet. Falls keine vorhanden sind, werden die Default-Werte herangezogen. Diese können natürlich überschrieben werden.
- Bei einer Änderung der Stammdaten wird des ESP - wenn dieser im AP-Modus betrieben wird - automatisch neu gestartet. Es wird dann in das konfigurierte WLAN eingewählt und der AP-Modus verlassen.
- 


