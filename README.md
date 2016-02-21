# engMQTTClient
MQTT client for Energenie [ener314-rt board] (https://energenie4u.co.uk/catalogue/product/ENER314-RT) running on a raspberry Pi

Please note that this is for the 'two way' ener314-rt board, which is different from 'one way' ener314 board, and does not work for the 'one way' board.

![ener314-rt board] (https://energenie4u.co.uk/res/images/products/medium/RXTX Board Front L.jpg)

engMQTTClient uses the mosquitto MQTT client library to do the MQTT handling.  Communication with the Energenie products is developed from the code in  the directory eTRV/eTRV\_TX/HopeRF-TX-RX

It also uses the log4c logging library.

## Status
Early development. 
I have tested : 
* communication from one eTRV. 
* receiving MQTT messages 
* sending ON/OFF commands to ENER002 remote controlled sockets.

## Building

Download and compile and install the latest versions of mosquitto and log4c.  The versions from the respository are too old to work.  I have mosquitto 1.4.7 and log4c 1.2.4 working.

Compile engMQTTClient using 'make'.

Run the program using

        sudo LD\_LIBRARY\_PATH=/usr/local/lib ./engMQTTClient

assuming mosquitto and log4c have been placed in /usr/local/lib as per default.

### MQTT Topic structure

For ENER002 sockets, using its own protocol the structure is
        /energenie/ENER002/_address_/_socketnum_

        _address_ is an (up to) 20 digit hex string 
        _socketnum_ is 0-4.  0 addresses all the sockets accessed by address, 1-4 access individual sockets at that address.

For eTRV (and hopefully other OpenThings protocol devices) the structure to send command to device is
        /energenie/eTRV/Command/_commandid_/_deviceid_

To receive commands the structure is
        /energenie/eTRV/Report/_commandid_/_deviceid_

        _commandid_ is the command sent or received (so far "Identity" or "Temperature")
        _deviceid_ is the openThing id number for the device in decimal
        
## Usage

### OpenHab example

#### Controlling switches

Item file

        Switch Light_FF_Bed_Table 	"Bedside Lamp" 	(FF_Bed, Lights)
         {mqtt=">[raspberryPI:/energenie/ENER002/8ee8ee888ee8ee888ee8/1:command:ON:On],>[raspberryPI:/energenie/ENER002/8ee8ee888ee8ee888ee8/1:command:OFF:Off]"}

Sitemap file
        
        Switch item=Light_FF_Bed_Table label="Bedroom Light" icon="switch"

#### Controlling eTRV

Item file

        Group Temperature_Chart 
        Number Temperature_329_set    "Lounge Temperature Target [%.1f °C]" <temperature> (Temperature_Chart, FF_Bed)   
                                      {mqtt=">[raspberryPi:/energenie/eTRV/Command/Temperature/329:command:*:${command}]"}
        Number Temperature_329        "Lounge Temperature [%.1f °C]"   <temperature> (Temperature, Temperature_Chart, FF_Bed)
                                      {mqtt="<[raspberryPi:/energenie/eTRV/Report/Temperature/329:state:default"}

        Number Temperature_Chart_Period		"Chart Period"

        Number eTRV_329_received_target   "Lounge Received Target" <temperature> (Trv, FF_Bed, Temperature_Chart)
                                          {mqtt="<[raspberryPi:/energenie/eTRV/Report/TargetTemperature/329:state:default"}
        Number eTRV_329               "Lounge TRV Control"    (Trv, FF_Bed)
                                          {mqtt=">[raspberryPi:/energenie/eTRV/Command/Identify/329:command:1:0]"}
                                          
Sitemap file

        Setpoint item=Temperature_329_set icon="temperature" minValue=4 maxValue=30 step=1
	Text item=Temperature_329
	Switch item=eTRV_329 mappings=[1="Identify"]
		
	Switch item=Temperature_Chart_Period label="Chart Period" mappings=[0="Hour", 1="Day", 2="Week"]
	Chart item=Temperature_Chart period=h refresh=6000 visibility=[Temperature_Chart_Period==0, Temperature_Chart_Period=="Uninitialized"]
	Chart item=Temperature_Chart period=D refresh=30000 visibility=[Temperature_Chart_Period==1]
	Chart item=Temperature_Chart period=W refresh=30000 visibility=[Temperature_Chart_Period==2]
	
## License
This code is published under the MIT License.  The last sentence is important.  If running this code causes your device to fail, I'm not responsible.

The mosquitto MQTT library is released under the Eclipse Public License.

The log4c logging library is released under the LGPL license.

