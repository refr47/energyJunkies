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
