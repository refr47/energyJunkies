#!/bin/bash
# Lastprofilgenerator benötigt 1 Eingabedatei (NetzOÖ-Export) als Argument und eine eventuelle Zeitverschiebung
if (( $# < 1 )); then
  echo "usage $0 <Netz OOE Verbrauchsaufstellung> [+02:00]"
  echo "2. Argument ist die Zeitverschiebung am Beginn der Zeitreihe bezüglich UTC (default: +01:00 für MEZ)"
  echo "Beispielaufruf: ./gridlog2loadprofile.sh netzOOE/verbrauch-202307-202406.csv +02:00 >profile/verbrauch-202307-202406.csv"
  exit 1
fi

# Netz-OOE-Export - erwarteter Dateiaufbau:
#Datum;kWh;kW;Status
#01.07.2023 00:00;0,001000;0,004000;VALID
#01.07.2023 00:15;0,001000;0,004000;VALID

# Lastprofil - erzeugter Dateiaufbau:
#Ende Ablesezeitraum;Messintervall;Abrechnungsmaßeinheit;Verbrauch
#2023-07-01T00:00+02:00;QH;KWH;0,001
#2023-07-01T00:15+02:00;QH;KWH;0,001


# Annahme: Zeitverschiebung ist 1 Stunde für MEZ
# ansonsten z.B. +02:00 für MESZ als 2. Argument angeben
verschiebung="+01:00"
if (( $# > 1 )); then
  verschiebung="$2"
fi

# Argumente übernehmen
datei="$1"

# Kopfzeile ausgeben
echo "Ende Ablesezeitraum;Messintervall;Abrechnungsmaßeinheit;Verbrauch"
vorher="00:00"

# Datei zeilenweise durchlaufen
while read zeile; do
  # jede Zeile überprüfen, ob sie eine Datenzeile ist (beginnt mit Datum)
  symbol=`echo $zeile|cut -c1`
  # nur aktive Zeilen (also Datenzeilen) verarbeiten
  if [[ "$symbol" != "D" ]]; then
    # Datum, Zeit, kWh, kW isolieren
    zeitstempel=`echo $zeile|cut -d';' -f1`
    kwh=`echo $zeile|cut -d';' -f2`
#    kw=`echo $zeile|cut -d';' -f3`
    valid=`echo $zeile|cut -d';' -f4`
    datum=`echo $zeitstempel|cut -d' ' -f1`
    tag=`echo $datum|cut -d'.' -f1`
    monat=`echo $datum|cut -d'.' -f2`
    jahr=`echo $datum|cut -d'.' -f3`
    zeit=`echo $zeitstempel|cut -d' ' -f2`
    wert=`echo $kwh|cut -c1-5`
    # nur gültige Einträge verarbeiten
    if [[ "$valid" == "VALID" ]]; then
      # Wechsel in die Sommerzeit (01:45 gefolgt von 03:00)
      if [[ "$vorher" == "01:45" && "$zeit" == "03:00" ]]; then
        verschiebung="+02:00"
      # Wechsel in die Normalzeit (02:45 gefolgt von 02:00)
      elif [[ "$vorher" == "02:45" && "$zeit" == "02:00" ]]; then
        verschiebung="+01:00"
      fi
      vorher="$zeit"
      echo "${jahr}-${monat}-${tag}T${zeit}${verschiebung};QH;KWH;${wert}"
    fi
  fi
done < "$datei"

