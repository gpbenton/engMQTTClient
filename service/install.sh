#!/bin/bash

# Set up engmqttclient as a systemd service

echo "Copying files..."
cp -i ../engMQTTClient /usr/bin/
chown root:root /usr/bin/engMQTTClient
cp -i engmqttclient.conf /etc/engmqttclient.conf 
chown root:root /etc/engmqttclient.conf
cp -i engMQTTClient_service.sh /usr/bin/engMQTTClient_service.sh
chown root:root /usr/bin/engMQTTClient_service.sh
cp -i engmqttclient.service /etc/systemd/system/engmqttclient.service
chown root:root /etc/systemd/system/engmqttclient.service

echo "Calling systemctl daemon-reload..."
systemctl daemon-reload

echo "Installation complete"
echo ""
echo "Now edit /etc/engmqttclient.conf to update with your local configuration."
echo ""
echo "To test the service use:"
echo "sudo systemctl start engmqttclient.service"
echo "To permanently enable the service use:"
echo "sudo systemctl enable engmqttclient.service"
