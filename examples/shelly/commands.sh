#!/bin/bash


#export SHELLY=10.33.55.152 # IP of your device
curl -X POST -d '{"id":1, "method":"Mqtt.GetStatus"}' http://${SHELLY}/rpc


curl -X POST -d '{"id":1, "method":"Mqtt.GetConfig"}' http://${SHELLY}/rpc


MQTT_SERVER="RaspiHA-Ethernet"
MQTT_PORT=1883
SHELLY_ID="shellyplus1-pool" # The <shelly-id> of your device
mosquitto_sub -h ${MQTT_SERVER} -p ${MQTT_PORT} -t ${SHELLY_ID}/events/rpc
