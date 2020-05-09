# engMQTTClient/docker
# by @setdetnet
# v1.0
# Last updated 9th May 2020


Docker setup to run @gpbenton's Energenie MQTT Client.
To install simply run the install.sh script; this basic script will create an /opt/energenie_mqtt_client directory, but this can be modified by setting the OPT value in the script.
Indeed, the full list of user config vars , which can be defined in the install.sh script, are:-


# Install to this directory
OPT=/opt/energenie_mqtt_client/

# Location for systemd files. Script assumes installing into a SystemV type environment.
SYSTEMD=/etc/systemd/system/

# Name of service file
SERVICE_FILE=docker-compose-energenie-mqtt.service

# Path to Docker Compose binary
DOCKER_COMPOSE_BIN=/usr/bin/docker-compose

# Path to docker-compose file
DOCKER_COMPOSE_YAML=docker-compose.yaml


