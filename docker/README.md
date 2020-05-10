# engMQTTClient/docker
# by @setdetnet
# v1.1
# Last updated 10th May 2020

Docker setup to run @gpbenton's Energenie MQTT Client.

To run from command line use

docker run --name engmqtt --privileged --network "host" --rm gpbenton/engmqttclient:1.0.0 <any parameters to engMQTTClient>

for example
docker run --name engmqtt --privileged --network "host" --rm gpbenton/engmqttclient:1.0.0 -h 192.168.0.3

The docker-compose file includes a container running a mosquitto mqtt broker.  Remove this if you already have a broker running.  To run this simply

docker-compose up
 
