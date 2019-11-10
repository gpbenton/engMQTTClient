#!/bin/bash
# Script to launch the energenie MQTT client
# Normally this script will be stored in /usr/bin/engMQTTClient_service.sh

source /etc/engmqttclient.conf

sleep 10s 
LD_LIBRARY_PATH=/usr/local/lib ${ENGMQTT_BIN} -h ${ENGMQTT_SERVER} -u ${ENGMQTT_USER} -P ${ENGMQTT_PASS}
echo "engmqtt completed with exit code $?"
