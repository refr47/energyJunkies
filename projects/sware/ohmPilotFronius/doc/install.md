# Content
Anleitung für das Flashen des Projekts (esp32) mit Hilfe von PlatformIO

## Voraussetzungen
Das Projekt via Github clonen; PlattformIO wird die gesamte Toolchain des esp32 installieren; zusätzlich werden die Abhängigkeiten (Libs) aufgelöst und heruntergeladen. Dies kann - je nach Internetverbindung - schon eine Zeit dauern.

## Compile/Flash
Zuerst muss das Projekt fehlerfrei übersetzt werden können und ein Image für den esp32 erzeugt werden. Es kommen noch einige Warnings, aber die sind eher bedeutungslos und kommen größtenteils von den verwendeten Libs.

Nach dem Übersetzen kann nun mit dem Flash begonnen werden. Dazu ist folgendes zu beachten

- Der Energie-Harvester ist mit einer REST-Schnittstelle ausgestattet, wobei der Client im "data"-Ordner liegt. Dieser Ordner enthält alle notwendigen Ressourcen für die WWW-Anbindung und wird vom Client (Browser) geladen. Die REST-Schnittstelle steht immer zur Verfügung und MUSS extra geflasht werden.
- Dazu muss auf der linken Seite im VS-Code Editor das PlattformIO Symbol ausgewählt werden und dann in den "Project Tasks" das Projekt "lilygo-t-display-s3" ausgewählt werden. Unter "Plattform" sieht man dann die Menüpunkte "Build Filesystem Image" und "Upload Filesystem Image". 
- Zuerst ist das FilesystemImage zu erstellen und dann zu flashen.
- Für die Ablage dieser Ressourcen wird das vom ESP-IF zur Verfügung gestellte Filesystem (SPIFFs) verwendet. Daher auch die Aufspaltung in 2 Flash-Vorgänge.
- Sodann kann das compilierte Image geflasht werden und der esp32 startet.

## Inbetriebname

Der E-Harvester speichert sich die Konfigurationsdaten im Flash-Speicher und die REST-Schnittstelle bedient sich dieser Daten und speichert dort diese auch wieder ab.

Es gibt da Stammdaten und Mobilitätsdaten und die Logfiles 
- Stammdaten: Alles, was man für den Betrieb so braucht (ESSID, PID-Einstellungen, ....)
- Mobilitätsdaten: Ein Teil der Informationen wird über das kleine Display ausgegeben, die meisten werden per WWW zur Verfügung gestellt. Es sind Life-daten (die Daten werden per "WebSockets" aktualisiert)
- Logfiles: Es wird einiges mitprotokolliert und auf dem CardReader gespeichert. Diese Logfiles können via Browser heruntergeladen werden.


### Access-Point Modus
- Falls sich der ESP nicht in das im Setup hinterlegte WLAN einloggen kann, geht er in den AP-Modus und stellt ein eigenes WLAN zur Verfügung; dieses wird auch am Display angezeigt und ist frei (d.h. kein Benuter/Password). 
- Default-Adresse: 192.168.4.1
- Default ESSID: Energy Junkies
- Nun kann man sich in dieses WLAN einwählen und obige IP-Adresse per Browser aufrufen. Man gelangt dann zu einem Login-Dialog, wobei hier der Fallback-Zugang mit **admin** und Password **password** hinterlegt ist.
- Nach dem Speichern wird der ESP32 kommentarlos neu gestartet. 

### Client Modus
* Wenn die hinterlegten Daten stimmen, kann sich der ESP32 in das WLAN einwählen und läuft einen Initialisierungsprozess durch, indem sämtliche Komponenten geprüft werden, ob diese vorhanden sind und funktionieren. Dies sind
        Netzwerk
        SPIFF-Filesystem 
        WebServer
        NTP (Zeitserver)
        CARD-Reader
        Logging-Ressourcen (bedingt gültigen CARD-Reader)
        Temperatur-Sensorik
        Modbus
