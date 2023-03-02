#!/bin/bash

base="/home/pi/strg-mngr"
name="`date +%Y%m%d`"

killall "/home/pi/bin/configreader"

sleep 5

cp "$base"/current.yaml "$base"/tariff/"${name}".yaml
curl http://api.awattar.at/v1/marketdata/current.yaml?tomorrow=include >"$base"/current.yaml

/home/pi/bin/configreader -i 10.0.0.13 -p 502 -x 1.00 -d 8.00 -f 0.75 -s 13 -o +60 -c "$base"/strg-mngr.conf -t "$base"/current.yaml 1>>"$base"/log/strg-mngr.log 2>>"$base"/log/strg-mngr.err



