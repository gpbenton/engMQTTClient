# engMQTTClient/docker
# by @setdetnet
# v1.1
# Last updated 10th May 2020

Docker setup to run @gpbenton's Energenie MQTT Client.

To run from command line use

docker run --name engmqtt --privileged --network "host" --rm gpbenton/engmqttclient:1.0.0 <any parameters to engMQTTClient>

for example
docker run --name engmqtt --privileged --network "host" --rm gpbenton/engmqttclient:1.0.0 -h 192.168.0.3

To stop this, in another window type
docker stop engmqtt

The docker-compose file includes a container running a mosquitto mqtt broker.  Remove this if you already have a broker running.  To run this simply

docker-compose up -d
 
To see the logs type
docker-compose logs

An example systemd service file is supplied in case you wish to use this instead of docker-compose, but isn't tested, so may need some tweeking.
