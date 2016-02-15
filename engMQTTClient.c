/*
The MIT License (MIT)

Copyright (c) 2016 gpbenton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <bcm2835.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <log4c.h>
#include <mosquitto.h>
#include "engMQTTClient.h"
#include "dev_HRF.h"
#include "decoder.h"

#define MQTT_TOPIC_BASE "energenie"
#define MQTT_TOPIC_ETRV       "eTRV"
#define MQTT_TOPIC_MIH005     "MIH005"
#define MQTT_TOPIC_RCVD_TEMP  "TemperatureReport"
#define MQTT_TOPIC_MAX_SENSOR_LENGTH  8          // length of string of largest sensorId
                                                 // 16,777,215

static const uint8_t engManufacturerId = 0x04;   // Energenie Manufacturer Id
static const uint8_t eTRVProductId = 0x3;        // Product ID for eTRV
static const uint8_t encryptId = 0xf2;           // Encryption ID for eTRV

static const char* mqttBrokerHost =  "localhost";  // TODO configuration for other set ups
static const int   mqttBrokerPort = 1883;
static const int keepalive = 60;
static const bool clean_session = true;

int err = 0;

enum fail_codes {
    ERROR_LOG4C_INIT=1,
    ERROR_MOSQ_NEW,
    ERROR_MOSQ_CONNECT,
    ERROR_MOSQ_LOOP_START,
    ERROR_ENER_INIT_FAIL
};


static log4c_category_t* clientlog = NULL;
static log4c_category_t* stacklog = NULL;
log4c_category_t* hrflog = NULL;


void my_message_callback(struct mosquitto *mosq, void *userdata, 
                         const struct mosquitto_message *message)
{
    char *payload = (char *)message->payload;
    char *topic = (char *)message->topic;

    log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "%s", __FUNCTION__);

    if(message->payloadlen){
        int socketNum;
        int onOff;
        int cmd;

        log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, 
                           "%s %s", topic, payload);

        socketNum = payload[0] - '0';
        if (socketNum < 1 || socketNum > 4) {
            log4c_category_error(clientlog, "Invalid socket number: %d",
                                 socketNum);
            return;
        }

        onOff = payload[1] - '0';
        onOff = (onOff)?0:1;
        cmd = ((socketNum -1) * 2) + onOff;

        log4c_category_debug(clientlog, "Sending %d to socket %d",
                             onOff, socketNum);

        HRF_send_OOK_msg(cmd);

    }else{
        log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, 
                           "%s (null)", message->topic);
    }
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "%s", __FUNCTION__);

    if(!result){
        log4c_category_log(clientlog, LOG4C_PRIORITY_NOTICE, 
                           "Connected to broker ");
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, "plugs", 2);
    }else{
        log4c_category_log(clientlog, LOG4C_PRIORITY_WARN, 
                           "Connect Failed with error %d", result);
    }
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid,
                           int qos_count, const int *granted_qos)
{
    log4c_category_log(clientlog, LOG4C_PRIORITY_NOTICE, 
                       "Subscribed (mid: %d): %d", mid, granted_qos[0]);
}

void my_log_callback(struct mosquitto *mosq, void *userdata, int level, 
                     const char *str)
{
    log4c_priority_level_t priority;

    switch (level) {
        case  MOSQ_LOG_INFO:
            priority = LOG4C_PRIORITY_INFO;
            break;

        case	MOSQ_LOG_NOTICE:
            priority = LOG4C_PRIORITY_NOTICE;
            break;
        case	MOSQ_LOG_WARNING:
            priority = LOG4C_PRIORITY_WARN;
            break;

        case	MOSQ_LOG_ERR:
            priority = LOG4C_PRIORITY_ERROR;
            break;

        case	MOSQ_LOG_DEBUG:
            priority = LOG4C_PRIORITY_DEBUG;
            break;

        default:
            priority = LOG4C_PRIORITY_WARN;
            break;
    }

    log4c_category_log(stacklog, priority, "%s", str);
}

// receive in variable length packet mode, display and resend. Data with swapped first 2 bytes
int main(int argc, char **argv){
    		
    extern	uint8_t recieve_temp_report;
    extern char received_temperature[MAX_DATA_LENGTH];
    struct mosquitto *mosq = NULL;
	
    if (log4c_init()) {
        fprintf(stderr, "log4c_init() failed");
        return ERROR_LOG4C_INIT;
    }

    clientlog = log4c_category_get("MQTTClient");
    stacklog = log4c_category_get("MQTTStack");
    hrflog = log4c_category_get("hrf");


	if (!bcm2835_init())
		return ERROR_ENER_INIT_FAIL;

	
	// LED INIT
	bcm2835_gpio_fsel(LEDG, BCM2835_GPIO_FSEL_OUTP);			// LED green
	bcm2835_gpio_fsel(LEDR, BCM2835_GPIO_FSEL_OUTP);			// LED red
	bcm2835_gpio_write(LEDG, LOW);
	bcm2835_gpio_write(LEDR, LOW);
	// SPI INIT
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(SPI_CLOCK_DIVIDER_26); 			// 250MHz / 26 = 9.6MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	HRF_config_FSK();
	HRF_wait_for(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait until ready after mode switching
	HRF_clr_fifo();

    mosquitto_lib_init();
    mosq = mosquitto_new("Energenie Controller", clean_session, NULL);
    if(!mosq){
        log4c_category_log(clientlog, LOG4C_PRIORITY_CRIT, "Out of memory");
        return ERROR_MOSQ_NEW;
    }
    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

    if((err = mosquitto_connect_async(mosq, mqttBrokerHost, mqttBrokerPort, keepalive)) 
       != MOSQ_ERR_SUCCESS){
        log4c_category_log(clientlog, LOG4C_PRIORITY_CRIT, 
                           "Unable to connect: %d", err);
        return ERROR_MOSQ_CONNECT;
    }



    if ((err = mosquitto_loop_start(mosq)) != MOSQ_ERR_SUCCESS) {
        log4c_category_log(clientlog, LOG4C_PRIORITY_CRIT,
                           "Loop start failed: %d", err);
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return ERROR_MOSQ_LOOP_START;
    }

	while (1){
        uint32_t rcvdSensorId;
		
		HRF_receive_FSK_msg(encryptId, eTRVProductId, engManufacturerId, &rcvdSensorId );

        if (send_join_response) {
            if ( join_manu_id == engManufacturerId &&
                 join_prod_id == eTRVProductId) {

                /* We got a join request for an eTRV */
                log4c_category_debug(clientlog, "send Join response for sensorId %d", rcvdSensorId);

                HRF_send_FSK_msg(
                                 HRF_make_FSK_msg(join_manu_id, encryptId, join_prod_id, join_sensor_id,
                                                  2, PARAM_JOIN_RESP, 0), 
                                 encryptId);
                send_join_response = FALSE;
            } else {
                log4c_category_info(clientlog, 
                        "Received Join message for ManufacturerId:%d ProductId:%d SensorId:%d", 
                        join_manu_id, join_prod_id, join_sensor_id);
            }
        }

        if (recieve_temp_report) {
            log4c_category_debug(clientlog, "send NIL command for sensorId %d", rcvdSensorId);
            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                              eTRVProductId, rcvdSensorId, 0), 
                             encryptId);
            log4c_category_info(clientlog, "Temperature=%s", received_temperature);

            char mqttTopic[sizeof(MQTT_TOPIC_BASE) + sizeof(MQTT_TOPIC_ETRV) 
                + sizeof(MQTT_TOPIC_RCVD_TEMP)  + MQTT_TOPIC_MAX_SENSOR_LENGTH + 4 + 1];
            snprintf(mqttTopic, sizeof(mqttTopic), "/%s/%s/%d/%s", MQTT_TOPIC_BASE, MQTT_TOPIC_ETRV,
                     rcvdSensorId, MQTT_TOPIC_RCVD_TEMP);
            mosquitto_publish(mosq, NULL, mqttTopic, 
                              strlen(received_temperature), received_temperature, 0, false);
            recieve_temp_report = FALSE;  
        }
			
/*         if (recieve_temp_report)
        {
      
            if (queued_data)
            {
                                
                printf("send temp report\n");
                HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, productId, sensorId,
                                              4, PARAM_TEMP_SET, 0x92, (data & 0xff), (data >> 8 & 0xff)), encryptId);
            queued_data = FALSE;
            recieve_temp_report = FALSE;
            
          } else {
            printf("send IDENTIFY command\n");
            HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, 
                                      productId, sensorId, 2, 0xBF , 0), encryptId);
            recieve_temp_report = FALSE;  
      }
      }                                                                                                                   */
		
		
	/*	if (!quiet && difftime(currentTime, monitorControlTime) > 1)
		{
			monitorControlTime = time(NULL);
			static bool switchState = false;
			switchState = !switchState;
			printf("send temp message:\trelay %s\n", switchState ? "ON" : "OFF");
			bcm2835_gpio_write(LEDG, switchState);
			HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, productId, sensorId,
											  4, PARAM_TEMP_SET, 0x92, 0x10, 0x20),
							 encryptId);
		}
		*/
        usleep(1000);
	}
	bcm2835_spi_end();
	return 0;
}


/* vim: set cindent sw=4 ts=4 expandtab path+=/usr/local/include : */
