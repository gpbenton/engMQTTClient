# Base image is gcc build env

FROM gcc:8.3.0 AS gcc_dev

# log4c

WORKDIR /opt/energenie_mqtt_client/log4c
RUN curl -SL http://prdownloads.sourceforge.net/log4c/log4c-1.2.4.tar.gz \
    |  tar -xzC . \
    && cd log4c-1.2.4 \
    && ./configure \
    && make && make install \
    && rm -rf /opt/energenie_mqtt_client/log4c
    
# bcm2835

WORKDIR /opt/energenie_mqtt_client/bcm2835
RUN curl -SL http://www.airspayce.com/mikem/bcm2835/bcm2835-1.64.tar.gz \
    |  tar -xzC . \
    && cd bcm2835-1.64 \
    && ./configure \
    && make && make install \
    && rm -rf /opt/energenie_mqtt_client/bcm2835

# mosquitto

# Setup mosquitto repo
# http://repo.mosquitto.org/debian/mosquitto-jessie.list
# http://repo.mosquitto.org/debian/mosquitto-stretch.list
# http://repo.mosquitto.org/debian/mosquitto-buster.list
WORKDIR /opt/energenie_mqtt_client/mosquitto
RUN curl -SL http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key | apt-key add - \
    && curl -SL http://repo.mosquitto.org/debian/mosquitto-buster.list  -o /etc/apt/sources.list.d/mosquitto.list \
    && apt update && apt install -y mosquitto mosquitto-dbgsym mosquitto-clients mosquitto-dev \
    && rm -rf /opt/energenie_mqtt_client/mosquitto

# MQTT client

WORKDIR /opt/energenie_mqtt_client
RUN git clone https://github.com/gpbenton/engMQTTClient.git \
    && cd engMQTTClient \
    && make \
    && cp engMQTTClient /opt/energenie_mqtt_client/engMQTTClient.exe \
    && cp log4crc /opt/energenie_mqtt_client/ \
    && rm -rf /opt/energenie_mqtt_client/engMQTTClient 

# FINISHED

ENTRYPOINT echo "Run using: systemctl start energenie-mqtt.service !"

#For testing when attaching via a shell
#ENTRYPOINT /usr/bin/tail -f /dev/null
