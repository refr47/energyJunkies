#!/bin/bash

base="/home/pi/strg-mngr"
name="`date +%Y%m%d`"

today=`date +%Y-%m-%d`
tomorrow=`date --date='next day' +%Y-%m-%d`
echo "$today" > "$base"/log/today.txt
echo "$tomorrow" > "$base"/log/tomorrow.txt

killall "/home/pi/bin/energy_mgr"

sleep 1

if [ ! -e "$base"/tariff/"${name}".yaml ]; then
  mv "$base"/current.yaml "$base"/tariff/"${name}".yaml
fi

curl http://api.awattar.at/v1/marketdata/current.yaml?tomorrow=include >"$base"/current.yaml
# check if tomorrow is included
linecount=`wc -l "$base"/current.yaml | cut -d' ' -f 1`
while [ "$linecount" -lt 100 ]; do
  echo "Retrying to get current.yaml" 2>>"$base"/log/strg-mngr.err
  sleep 1m
  curl http://api.awattar.at/v1/marketdata/current.yaml?tomorrow=include >"$base"/current.yaml
  linecount=`wc -l "$base"/current.yaml | cut -d' ' -f 1`
done

/home/pi/bin/energy_mgr -i 10.0.0.13 -p 502 -x 1.00 -d 8.00 -f 0.75 -s 9 -o +60 -v 3 -c "$base"/strg-mngr.conf -t "$base"/current.yaml 1>>"$base"/log/strg-mngr.log 2>>"$base"/log/strg-mngr.err
