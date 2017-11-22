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
#include <sys/queue.h>
#include <pthread.h>
#include <ctype.h>
#include "engMQTTClient.h"
#include "dev_HRF.h"
#include "OpenThings.h"
#include "decoder.h"
#include "cJSON.h"

/* MQTT Definitions */

#define MQTT_TOPIC_BASE "energenie"

/* eTRV Topics */
#define MQTT_TOPIC_ETRV       "eTRV"
#define MQTT_TOPIC_COMMAND    "Command"
#define MQTT_TOPIC_REPORT     "Report"

#define MQTT_TOPIC_ETRV_COMMAND "/" MQTT_TOPIC_BASE "/" MQTT_TOPIC_ETRV "/" MQTT_TOPIC_COMMAND
#define MQTT_TOPIC_ETRV_REPORT  "/" MQTT_TOPIC_BASE "/" MQTT_TOPIC_ETRV "/" MQTT_TOPIC_REPORT

#define MQTT_TOPIC_BASE_INDEX 1
#define MQTT_TOPIC_DEVICE_INDEX 2
#define MQTT_TOPIC_COMMAND_INDEX 3

#define MQTT_TOPIC_TEMPERATURE  "Temperature"           /* OT_TEMP_SET */
#define MQTT_TOPIC_IDENTIFY     "Identify"              /* OT_IDENTIFY */
#define MQTT_TOPIC_EXERCISE_VALVE "Exercise"            /* OT_EXERCISE_VALVE */
#define MQTT_TOPIC_VOLTAGE "Voltage"                    /* OT_REQUEST_VOLTAGE */
#define MQTT_TOPIC_DIAGNOSTICS "Diagnostics"            /* OT_REQUEST_DIAGNOTICS */
#define MQTT_TOPIC_VALVE_STATE "ValveState"             /* OT_SET_VALVE_STATE */
#define MQTT_TOPIC_POWER_MODE  "PowerMode"              /* OT_SET_LOW_POWER_MODE */
#define MQTT_TOPIC_REPORTING_INTERVAL "ReportingInterval" /* OT_SET_REPORTING_INTERVAL */

#define MQTT_TOPIC_TARGET_TEMPERATURE "TargetTemperature"

#define MQTT_TOPIC_RCVD_TEMP_COMMAND MQTT_TOPIC_ETRV_COMMAND "/" MQTT_TOPIC_TEMPERATURE
#define MQTT_TOPIC_SENT_TEMP_REPORT  MQTT_TOPIC_ETRV_REPORT "/" MQTT_TOPIC_TEMPERATURE
#define MQTT_TOPIC_SENT_TARGET_TEMP     MQTT_TOPIC_ETRV_REPORT "/" MQTT_TOPIC_TARGET_TEMPERATURE
#define MQTT_TOPIC_RCVD_VALVE_STATE  MQTT_TOPIC_ETRV_COMMAND "/" MQTT_TOPIC_VALVE_STATE
#define MQTT_TOPIC_SENT_VOLTAGE_REPORT MQTT_TOPIC_ETRV_REPORT "/" MQTT_TOPIC_VOLTAGE
#define MQTT_TOPIC_SENT_DIAGNOSTICS_REPORT MQTT_TOPIC_ETRV_REPORT "/" MQTT_TOPIC_DIAGNOSTICS

#define MQTT_TOPIC_TYPE_INDEX 4
#define MQTT_TOPIC_SENSORID_INDEX 5

#define MQTT_TOPIC_ETRV_COUNT (MQTT_TOPIC_SENSORID_INDEX + 1)

#define MQTT_TOPIC_MAX_SENSOR_LENGTH  8          // length of string of largest sensorId
                                                 // 16777215 (0xffffff)
/* ENER002 Topics */
#define MQTT_TOPIC_ENER002    "ENER002"
#define MQTT_TOPIC_ENER002_COMMAND     "/" MQTT_TOPIC_BASE "/" MQTT_TOPIC_ENER002

#define MQTT_TOPIC_OOK_ADDRESS_INDEX 3
#define MQTT_TOPIC_OOK_SOCKET_INDEX 4
#define MQTT_TOPIC_ENER002_COUNT (MQTT_TOPIC_OOK_SOCKET_INDEX + 1)

// host and port can be overriden at run time
static char* mqttBrokerHost =  "localhost";  // TODO improve hacky configuration for other set ups
static int   mqttBrokerPort = 1883;
static const int keepalive = 60;
static const bool clean_session = true;

/* OpenThings definitions */
static const uint8_t engManufacturerId = 0x04;   // Energenie Manufacturer Id
static const uint8_t eTRVProductId = 0x3;        // Product ID for eTRV
static const uint8_t encryptId = 0xf2;           // Encryption ID for eTRV

static int err = 0;

/* Options */
static int repeat_send = 8;                     // The number of times to 
                                                // send an ook message

static pthread_mutex_t sensorListMutex;
static TAILQ_HEAD(tailhead, entry) sensorListHead;

struct tailhead  *headp;

struct entry {
    TAILQ_ENTRY(entry) entries;
    int sensorId;
    uint8_t command;
    uint32_t data;
};

enum fail_codes {
    ERROR_LOG4C_INIT=1,
    ERROR_MOSQ_NEW,
    ERROR_MOSQ_CONNECT,
    ERROR_MOSQ_LOOP_START,
    ERROR_ENER_INIT_FAIL,
    ERROR_INVALID_PARAM
};


static log4c_category_t* clientlog = NULL;
static log4c_category_t* stacklog = NULL;
log4c_category_t* hrflog = NULL;

/* Adds a command and data to the list of things to be sent
 * to an OpenThings type device
 * TODO:  Prioritize IDENTITY commands
 */
void addCommandToSend(int deviceId, uint8_t command, uint32_t value) {

    struct entry *newEntry;
    struct entry *p;


    pthread_mutex_lock(&sensorListMutex);
    /* Find an existing entry in the queue and replace that */
    for (p = sensorListHead.tqh_first; p != NULL; p = p->entries.tqe_next) {
        if (p->sensorId == deviceId && p->command == command) {
            log4c_category_debug(clientlog, "Replacing existing command with %d:%x:%d",
                                 deviceId, command, value);
            p->data = value;
            pthread_mutex_unlock(&sensorListMutex);
            return;
        }
    }
    pthread_mutex_unlock(&sensorListMutex);

    /* No equivalent found, so create a new entry */

    newEntry = malloc(sizeof(struct entry));
    newEntry->sensorId = deviceId;
    newEntry->command = command;
    newEntry->data = value;

    log4c_category_debug(clientlog, "Adding command to send %d:%x:%d", deviceId, command, value);
    pthread_mutex_lock(&sensorListMutex);
    TAILQ_INSERT_TAIL(&sensorListHead, newEntry, entries);
    pthread_mutex_unlock(&sensorListMutex);
}

struct entry * findCommandToSend(int deviceId) {

    struct entry *p;

    for (p = sensorListHead.tqh_first; p != NULL; p = p->entries.tqe_next) {
        if (p->sensorId == deviceId) {
            log4c_category_debug(clientlog, "Removing command to send %d:%x:%d", 
                                 p->sensorId, p->command, p->data);
            pthread_mutex_lock(&sensorListMutex);
            TAILQ_REMOVE(&sensorListHead, p, entries);
            pthread_mutex_unlock(&sensorListMutex);
            return p;
        }
    }
    // No commands
    log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "No Commands to send");
    return NULL;
}

/* Converts hex string in hex to equivalent bytes
 * in bytes.
 * No input checking performed.  hex is expected
 * to contain the exact number of characters 
 * intended for output.
 */
void hexToBytes(uint8_t *bytes, char *hex) {

    int hexlength = strlen(hex);
    int bytelength = hexlength/2;
    int i;

    for (i = 0; i < hexlength; ++i) {
        int high = hex[i*2];
        int low = hex[(i*2) + 1];
        high = (high & 0xf) + ((high & 0x40) >> 6) * 9;
        low = (low & 0xf) + ((low & 0x40) >> 6) * 9;

        bytes[i] = (high << 4) | low;
    }

    if (log4c_category_is_trace_enabled(clientlog)) {
        int HEX_LOG_BUF_SIZE =  bytelength * 8;
        char logBuffer[HEX_LOG_BUF_SIZE];
        
        int logBufferUsedCount = 0;

        for (i=0; i < bytelength; ++i) {
            logBufferUsedCount += 
                snprintf(&logBuffer[logBufferUsedCount],
                         HEX_LOG_BUF_SIZE - logBufferUsedCount,
                         "[%d]=%02x%c", i, bytes[i], i%8==7?'\n':'\t');
        }

        logBuffer[HEX_LOG_BUF_SIZE - 1] = '\0';

        log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, 
                           "Converted %s to \n%s",
                           hex,
                           logBuffer);
    }
}

/*
 * Creates a JSON object representing the data in the 
 * diagnostic data array
 */
cJSON * createDiagnosticDataJson(uint8_t *diagnosticData) {
    static const uint8_t D0_MASK = 0x01;
    static const uint8_t D1_MASK = 0x02;
    static const uint8_t D2_MASK = 0x04;
    static const uint8_t D3_MASK = 0x08;
    static const uint8_t D4_MASK = 0x10;
    static const uint8_t D5_MASK = 0x20;
    static const uint8_t D6_MASK = 0x40;
    static const uint8_t D7_MASK = 0x80;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    cJSON_AddItemToObject(root, "Motor current below expectation", cJSON_CreateBool(*diagnosticData & D0_MASK));
    cJSON_AddItemToObject(root, "Motor current always high", cJSON_CreateBool(*diagnosticData & D1_MASK));
    cJSON_AddItemToObject(root, "Motor taking too long", cJSON_CreateBool(*diagnosticData & D2_MASK));
    cJSON_AddItemToObject(root, "discrepancy between air and pipe sensors", cJSON_CreateBool(*diagnosticData & D3_MASK));
    cJSON_AddItemToObject(root, "air sensor out of expected range", cJSON_CreateBool(*diagnosticData & D4_MASK));
    cJSON_AddItemToObject(root, "pipe sensor out of expected range", cJSON_CreateBool(*diagnosticData & D5_MASK));
    cJSON_AddItemToObject(root, "low power mode is enabled", cJSON_CreateBool(*diagnosticData & D6_MASK));
    cJSON_AddItemToObject(root, "no target temperature has been set by host", cJSON_CreateBool(*diagnosticData & D7_MASK));
    diagnosticData++;

    cJSON_AddItemToObject(root, "valve may be sticking", cJSON_CreateBool(*diagnosticData & D0_MASK));
    cJSON_AddItemToObject(root, "valve exercise was successful", cJSON_CreateBool(*diagnosticData & D1_MASK));
    cJSON_AddItemToObject(root, "valve exercise was unsuccessful", cJSON_CreateBool(*diagnosticData & D2_MASK));
    cJSON_AddItemToObject(root, "driver micro has suffered a watchdog reset and needs data refresh", cJSON_CreateBool(*diagnosticData & D3_MASK));
    cJSON_AddItemToObject(root, "driver micro has suffered a noise reset and needs data refresh", cJSON_CreateBool(*diagnosticData & D4_MASK));
    cJSON_AddItemToObject(root, "battery voltage has fallen below 2p2V and valve has been opened", cJSON_CreateBool(*diagnosticData & D5_MASK));
    cJSON_AddItemToObject(root, "request for heat messaging is enabled", cJSON_CreateBool(*diagnosticData & D6_MASK));
    cJSON_AddItemToObject(root, "request for heat", cJSON_CreateBool(*diagnosticData & D7_MASK));

    return root;
}

void my_message_callback(struct mosquitto *mosq, void *userdata, 
                         const struct mosquitto_message *message)
{
    char **topics;
    int topic_count;
    int i;

    log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "%s", __FUNCTION__);

    if (mosquitto_sub_topic_tokenise(message->topic, &topics, &topic_count) 
        != 
        MOSQ_ERR_SUCCESS) {
        log4c_category_error(clientlog, "Unable to tokenise topic");
        return;
    }

    if (log4c_category_is_trace_enabled(clientlog)) {
        for (i=0; i<topic_count; i++) {
            log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE,
                               "%d: %s", i, topics[i]);
        }
    }

    if (topic_count < 2) {
        log4c_category_error(clientlog, "Invalid Topic count %d", topic_count);
        mosquitto_sub_topic_tokens_free(&topics, topic_count);
        return;
    }

    if (strcmp(MQTT_TOPIC_BASE, topics[MQTT_TOPIC_BASE_INDEX]) != 0) {
        log4c_category_error(clientlog, "Received base topic %s", topics[MQTT_TOPIC_BASE_INDEX]);
        mosquitto_sub_topic_tokens_free(&topics, topic_count);
        return;
    }

    if (strcmp(MQTT_TOPIC_ENER002, topics[MQTT_TOPIC_DEVICE_INDEX]) == 0) {
        // Message for plug in socket type ENER002

        if (topic_count != MQTT_TOPIC_ENER002_COUNT) {
            log4c_category_error(clientlog, "Invalid topic count(%d) for %s", 
                                 topic_count,
                                 MQTT_TOPIC_ENER002);
            mosquitto_sub_topic_tokens_free(&topics, topic_count);
            return;
        }

#define MAX_OOK_ADDRESS_TOPIC_LENGTH (OOK_MSG_ADDRESS_LENGTH * 2)
#define MAX_OOK_SOCKET_TOPIC_LENGTH  1

        char address[MAX_OOK_ADDRESS_TOPIC_LENGTH + 1];
        char device[MAX_OOK_SOCKET_TOPIC_LENGTH + 1];

        strncpy(address, topics[MQTT_TOPIC_OOK_ADDRESS_INDEX], MAX_OOK_ADDRESS_TOPIC_LENGTH);
        address[MAX_OOK_ADDRESS_TOPIC_LENGTH] = '\0';
        strncpy(device, topics[MQTT_TOPIC_OOK_SOCKET_INDEX], MAX_OOK_SOCKET_TOPIC_LENGTH);
        device[MAX_OOK_SOCKET_TOPIC_LENGTH] = '\0';

        log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "Freeing tokens");

        mosquitto_sub_topic_tokens_free(&topics, topic_count);

        log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "Tokens Freed");


        if(message->payloadlen == 0) {
            log4c_category_error(clientlog, "No Payload for %s", 
                                 MQTT_TOPIC_ENER002);
            return;
        }

        int onOff;
        int socketNum;

        if (strcasecmp("On", message->payload) == 0) {
            onOff = 1;
        } else if (strcasecmp("Off", message->payload) == 0) {
            onOff = 0;
        } else {
            log4c_category_error(clientlog, "Invalid Payload for %s", 
                                 MQTT_TOPIC_ENER002);
            return;
        }

        socketNum = *device - '0';
        if (socketNum < 0 || socketNum > 4) {
            log4c_category_error(clientlog, "Invalid socket number: %d",
                                 socketNum);
            return;
        }

        /* Check we only have hex digits in address */
        int addressNumber = atoi(address);

        if (addressNumber > 0xFFFFF) {
            log4c_category_error(clientlog, "Invalid address, must be less than 1048576: %s",
                                 address);
            return;
        }

        uint8_t addressBytes[OOK_MSG_ADDRESS_LENGTH];
        int i;

        for (i = OOK_MSG_ADDRESS_LENGTH - 1; i>=0; --i) {
            int lownibble = (addressNumber & 0x01) ? 0x0E : 0x08;
            int highnibble = (addressNumber & 0x02) ? 0xE0 : 0x80;
            addressBytes[i] = highnibble | lownibble;
            addressNumber = addressNumber >> 2;
        }

        HRF_send_OOK_msg(addressBytes, socketNum, onOff, repeat_send);

    } else if (strcmp(MQTT_TOPIC_ETRV, topics[MQTT_TOPIC_DEVICE_INDEX]) == 0) {
        // Message for eTRV Radiator Valve
        
        if (topic_count != MQTT_TOPIC_ETRV_COUNT) {
            log4c_category_error(clientlog, "Invalid topic count(%d) for %s", 
                                 topic_count,
                                 MQTT_TOPIC_ETRV);
            mosquitto_sub_topic_tokens_free(&topics, topic_count);
            return;
        }
        

        if (strcmp(MQTT_TOPIC_COMMAND, topics[MQTT_TOPIC_COMMAND_INDEX]) != 0) {
            log4c_category_error(clientlog, "Invalid command %s for %s", topics[3], topics[2]);
            mosquitto_sub_topic_tokens_free(&topics, topic_count);
            return;
        }


        if (strcmp(MQTT_TOPIC_IDENTIFY, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {
            // Send Identify command to eTRV
            //
            char sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH + 1];
            strncpy(sensorId, topics[MQTT_TOPIC_SENSORID_INDEX], MQTT_TOPIC_MAX_SENSOR_LENGTH);
            sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH] = '\0';

            mosquitto_sub_topic_tokens_free(&topics, topic_count);

            int intSensorId = atoi(sensorId);

            if (intSensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", sensorId);
                return;
            }


            addCommandToSend(intSensorId, OT_IDENTIFY, 0);

        } else if (strcmp(MQTT_TOPIC_TEMPERATURE, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {
            // Send set temperature command to eTRV
            char sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH + 1];
            strncpy(sensorId, topics[MQTT_TOPIC_SENSORID_INDEX], MQTT_TOPIC_MAX_SENSOR_LENGTH);
            sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH] = '\0';

            mosquitto_sub_topic_tokens_free(&topics, topic_count);

            int intSensorId = atoi(sensorId);

            if (intSensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", sensorId);
                return;
            }

            if (message->payloadlen > 6) {
                log4c_category_error(clientlog, "Payload for set temperature must be less than "
                                                "7 bytes in length");
                return;
            }

            char tempString[message->payloadlen+1];
            strncpy(tempString, message->payload, message->payloadlen);
            tempString[message->payloadlen] = '\0';

            uint32_t temperature = strtoul(tempString, NULL, 0);

            if (temperature < 4 || temperature > 30) {
                log4c_category_error(clientlog, "Temperature must be between 4 and 30, got %d",
                                     temperature);
                return;
            }

            addCommandToSend(intSensorId, OT_TEMP_SET, temperature);
        } else if (strcmp(MQTT_TOPIC_VALVE_STATE, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {
            // Send set valve state command to eTRV
            char sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH + 1];
            strncpy(sensorId, topics[MQTT_TOPIC_SENSORID_INDEX], MQTT_TOPIC_MAX_SENSOR_LENGTH);
            sensorId[MQTT_TOPIC_MAX_SENSOR_LENGTH] = '\0';

            mosquitto_sub_topic_tokens_free(&topics, topic_count);

            int intSensorId = atoi(sensorId);

            if (intSensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", sensorId);
                return;
            }

            if (message->payloadlen != 1) {
                log4c_category_error(clientlog, "Payload for set valve state must be 1"
                                                "digit in length");
                return;
            }

            char tempString[message->payloadlen+1];
            strncpy(tempString, message->payload, message->payloadlen);
            tempString[message->payloadlen] = '\0';

            uint32_t state = strtoul(tempString, NULL, 0);

            if ( state > 2) {
                log4c_category_error(clientlog, "state must be between 0 and 2, got %d",
                                     state);
                return;
            }

            addCommandToSend(intSensorId, OT_SET_VALVE_STATE, state);


        } else if (strcmp(MQTT_TOPIC_POWER_MODE, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {
            // Send set valve state command to eTRV
            int sensorId = atoi(topics[MQTT_TOPIC_SENSORID_INDEX]);

            mosquitto_sub_topic_tokens_free(&topics, topic_count);

            if (sensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %d", sensorId);
                return;
            }

            if (message->payloadlen != 1) {
                log4c_category_error(clientlog, "Payload for set valve state must be 1"
                                                "digit in length");
                return;
            }

            char tempString[message->payloadlen+1];
            strncpy(tempString, message->payload, message->payloadlen);
            tempString[message->payloadlen] = '\0';

            uint32_t powerMode = strtoul(tempString, NULL, 0);

            if ( powerMode > 1) {
                log4c_category_error(clientlog, "Power Mode must be between 0 or 1, got %d",
                                     powerMode);
                return;
            }

            addCommandToSend(sensorId, OT_SET_LOW_POWER_MODE, powerMode);

        } else if (strcmp(MQTT_TOPIC_REPORTING_INTERVAL, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {
            // Send set valve state command to eTRV
            int sensorId = atoi(topics[MQTT_TOPIC_SENSORID_INDEX]);

            mosquitto_sub_topic_tokens_free(&topics, topic_count);

            if (sensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %d", sensorId);
                return;
            }

            if (message->payloadlen > 5 || message->payloadlen < 1) {
                log4c_category_error(clientlog, "Payload for set reporting interval must be 1 and 5"
                                                "digits in length");
                return;
            }

            char tempString[message->payloadlen+1];
            strncpy(tempString, message->payload, message->payloadlen);
            tempString[message->payloadlen] = '\0';

            uint32_t reportingInterval = strtoul(tempString, NULL, 0);

            if ( reportingInterval > 3600 || reportingInterval < 300) {
                log4c_category_error(clientlog, "Power Mode must be between 300 and 3600, got %d",
                                     reportingInterval);
                return;
            }

            addCommandToSend(sensorId, OT_SET_REPORTING_INTERVAL, reportingInterval);


        } else if (strcmp(MQTT_TOPIC_DIAGNOSTICS, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {

            int sensorId = atoi(topics[MQTT_TOPIC_SENSORID_INDEX]);

            mosquitto_sub_topic_tokens_free(&topics, topic_count);


            if (sensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", 
                                     topics[MQTT_TOPIC_SENSORID_INDEX]);
                return;
            }

            addCommandToSend(sensorId, OT_REQUEST_DIAGNOTICS, 0);
            
        } else if (strcmp(MQTT_TOPIC_EXERCISE_VALVE, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {

            int intSensorId = atoi(topics[MQTT_TOPIC_SENSORID_INDEX]);

            mosquitto_sub_topic_tokens_free(&topics, topic_count);


            if (intSensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", 
                                     topics[MQTT_TOPIC_SENSORID_INDEX]);
                return;
            }

            addCommandToSend(intSensorId, OT_EXERCISE_VALVE, 0);
            
        } else if (strcmp(MQTT_TOPIC_VOLTAGE, topics[MQTT_TOPIC_TYPE_INDEX]) == 0) {

            int intSensorId = atoi(topics[MQTT_TOPIC_SENSORID_INDEX]);

            mosquitto_sub_topic_tokens_free(&topics, topic_count);


            if (intSensorId == 0) {
                // Assume 0 isn't valid sensor id
                log4c_category_error(clientlog, "SensorId must be an integer: %s", 
                                     topics[MQTT_TOPIC_SENSORID_INDEX]);
                return;
            }

            addCommandToSend(intSensorId, OT_REQUEST_VOLTAGE, 0);
            
        } else {
            log4c_category_warn(clientlog, 
                           "Can't handle %s commands for %s yet", topics[4], topics[2]);
            mosquitto_sub_topic_tokens_free(&topics, topic_count);
        }


    }else{
        log4c_category_warn(clientlog, 
                           "Can't handle messages for %s yet", topics[2]);
    }

}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    log4c_category_log(clientlog, LOG4C_PRIORITY_TRACE, "%s", __FUNCTION__);

    if(!result){
        log4c_category_log(clientlog, LOG4C_PRIORITY_NOTICE, 
                           "Connected to broker at %s", mqttBrokerHost);
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_ENER002_COMMAND "/#", 2);

        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_ETRV_COMMAND "/#", 2);
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
    		
    struct mosquitto *mosq = NULL;
    struct ReceivedMsgData msgData;
    int c;
	
    if (log4c_init()) {
        fprintf(stderr, "log4c_init() failed");
        return ERROR_LOG4C_INIT;
    }

    clientlog = log4c_category_get("MQTTClient");
    stacklog = log4c_category_get("MQTTStack");
    hrflog = log4c_category_get("hrf");

    while ((c = getopt (argc, argv, "r:h:p:")) != -1) {
        switch (c) {
            case 'r':
                repeat_send = atoi(optarg);
                if (repeat_send == 0) {
                    log4c_category_crit(clientlog, "repeat_send must be an integer");
                    return ERROR_INVALID_PARAM;
                }
                break;
	    // hack basic support for overriding host and port
	    case 'h':
		mqttBrokerHost = optarg;
		break;
            case 'p':
		mqttBrokerPort = atoi(optarg);
	        break;
            default:
                log4c_category_crit(clientlog, "Invalid parameter");
                return ERROR_INVALID_PARAM;
        }
    }
                

    TAILQ_INIT(&sensorListHead);

	if (!bcm2835_init()) {
        log4c_category_crit(clientlog, "bcm2835_init() failed");
		return ERROR_ENER_INIT_FAIL;
    }
	
	// LED INIT
	bcm2835_gpio_fsel(greenLED, BCM2835_GPIO_FSEL_OUTP);			// LED green
	bcm2835_gpio_fsel(redLED, BCM2835_GPIO_FSEL_OUTP);			// LED red
    ledControl(greenLED, ledOff);
    ledControl(redLED, ledOn);

	// RESET
	bcm2835_gpio_fsel(RESET_PIN, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(RESET_PIN, HIGH);
	usleep(10000);
	bcm2835_gpio_write(RESET_PIN, LOW);
	usleep(10000);

	// SPI INIT
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(SPI_CLOCK_DIVIDER_9p6MHZ); 		
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

    ledControl(redLED, ledOff);
    ledControl(greenLED, ledOn);

    // clear all the flags from the message data
    memset(&msgData, 0, sizeof(msgData));
    while (1){

        HRF_receive_FSK_msg(encryptId, eTRVProductId, engManufacturerId, &msgData );

        if (msgData.msgAvailable) {
            if (msgData.joinCommand) {
                if ( msgData.manufId == engManufacturerId &&
                     msgData.prodId == eTRVProductId) {

                    /* We got a join request for an eTRV */
                    log4c_category_debug(clientlog, "send Join response for sensorId %d", msgData.sensorId);

                    HRF_send_FSK_msg(
                                     HRF_make_FSK_msg(msgData.manufId, encryptId, 
                                                      msgData.prodId, msgData.sensorId,
                                                      2, OT_JOIN_RESP, 0), 
                                     encryptId);
                } else {
                    log4c_category_notice(clientlog, 
                                          "Received Join message for ManufacturerId:%d ProductId:%d SensorId:%d", 
                                          msgData.manufId, msgData.prodId, msgData.sensorId);
                }
            }

            if (msgData.receivedTempReport) {
                struct entry *commandToSend = findCommandToSend(msgData.sensorId);

                if (commandToSend) {
                    switch (commandToSend->command) {
                        case OT_IDENTIFY:
                            log4c_category_debug(clientlog, "Sending Identify to device %d", 
                                                 msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId, 
                                                              2, OT_IDENTIFY, 0),
                                             encryptId);
                            break;

                        case OT_TEMP_SET:
                            log4c_category_debug(clientlog, "Sending Set Temperature %d to device %d",
                                                 commandToSend->data, msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              4, OT_TEMP_SET, 0x92, 
                                                              (commandToSend->data & 0xff), 
                                                              0x00), 
                                             encryptId);

                            {
                                // Report temperature set to MQTT broker
                                char mqttTempSetTopic[strlen(MQTT_TOPIC_SENT_TARGET_TEMP) 
                                    + MQTT_TOPIC_MAX_SENSOR_LENGTH 
                                    + 5 + 1];

                                snprintf(mqttTempSetTopic, sizeof(mqttTempSetTopic), "%s/%d", 
                                         MQTT_TOPIC_SENT_TARGET_TEMP, msgData.sensorId);

                                // Should only be 1 or 2 digits for temperature
                                char temperature[5];
                                snprintf(temperature, 4, "%d", commandToSend->data);

                                mosquitto_publish(mosq, NULL, mqttTempSetTopic, 
                                                  strlen(temperature), temperature,
                                                  0, false);
                            }
                            break;

                        case OT_EXERCISE_VALVE:
                            log4c_category_notice(clientlog, "Excercise Valve for sensorId %d", msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              2, OT_EXERCISE_VALVE, 0x00 
                                                             ), 
                                             encryptId);
                            break;

                        case OT_REQUEST_VOLTAGE:
                            log4c_category_notice(clientlog, "Request Voltage for sensorId %d", msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              2, OT_REQUEST_VOLTAGE, 0x00 
                                                             ), 
                                             encryptId);
                            break;

                        case OT_REQUEST_DIAGNOTICS:
                            log4c_category_notice(clientlog, "Request Diagnostics from device %d",
                                                  msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              2, OT_REQUEST_DIAGNOTICS, 0x00 
                                                             ), 
                                             encryptId);
                            break;

                        case OT_SET_VALVE_STATE:
                            log4c_category_notice(clientlog, "Set Valve State %d to sensorId %d",
                                                  commandToSend->data, msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              3, OT_SET_VALVE_STATE, 0x01, 
                                                              (commandToSend->data & 0xff)
                                                             ), 
                                             encryptId);
                            break;

                        case OT_SET_LOW_POWER_MODE:
                            log4c_category_notice(clientlog, "Set Low Power Mode %d to sensorId %d",
                                                  commandToSend->data, msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              3, OT_SET_LOW_POWER_MODE, 0x01, 
                                                              (commandToSend->data & 0xff)
                                                             ), 
                                             encryptId);
                            break;

                        case OT_SET_REPORTING_INTERVAL:
                            log4c_category_notice(clientlog, "Set Reporting Interval %d to sensorId %d",
                                                  commandToSend->data, msgData.sensorId);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId,
                                                              3, OT_SET_REPORTING_INTERVAL, 0x02, 
                                                              (commandToSend->data & 0xff),
                                                              ((commandToSend->data >> 8) & 0xff)
                                                             ), 
                                             encryptId);
                            break;


                        default:
                            log4c_category_warn(clientlog, "Don't understand command to send %x", 
                                                commandToSend->command);
                            HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                              eTRVProductId, msgData.sensorId, 0), 
                                             encryptId);
                            break;
                    }
                    free(commandToSend);
                } else {

                    log4c_category_debug(clientlog, "send NIL command for sensorId %d", msgData.sensorId);
                    HRF_send_FSK_msg(HRF_make_FSK_msg(engManufacturerId, encryptId, 
                                                      eTRVProductId, msgData.sensorId, 0), 
                                     encryptId);
                }


                log4c_category_info(clientlog, "SensorId=%d Temperature=%s", 
                                    msgData.sensorId, msgData.receivedTemperature);

                char mqttTopic[strlen(MQTT_TOPIC_SENT_TEMP_REPORT) 
                    + MQTT_TOPIC_MAX_SENSOR_LENGTH 
                    + 5 + 1];

                snprintf(mqttTopic, sizeof(mqttTopic), "%s/%d", 
                         MQTT_TOPIC_SENT_TEMP_REPORT, msgData.sensorId);
                mosquitto_publish(mosq, NULL, mqttTopic, 
                                  strlen(msgData.receivedTemperature), msgData.receivedTemperature, 
                                  1, false);
            }

            if (msgData.receivedDiagnostics) {
                char mqttTopic[strlen(MQTT_TOPIC_SENT_DIAGNOSTICS_REPORT) 
                    + MQTT_TOPIC_MAX_SENSOR_LENGTH 
                    + 5 + 1];
                cJSON *root;
                char *jsonString;

                log4c_category_notice(clientlog, "SensorId=%d Diagnostics 0:%x 1:%x", 
                                      msgData.sensorId, 
                                      msgData.diagnosticData[0], 
                                      msgData.diagnosticData[1]);

                root = createDiagnosticDataJson(msgData.diagnosticData);
                if (root == NULL) {
                    log4c_category_error(clientlog, "Unable to create Diagnostic Data JSON object");
                } else {
                    snprintf(mqttTopic, sizeof(mqttTopic), "%s/%d", 
                             MQTT_TOPIC_SENT_DIAGNOSTICS_REPORT, msgData.sensorId);
                    jsonString = cJSON_Print(root);
                    log4c_category_debug(clientlog, "Diagnostics %s", jsonString);
                    mosquitto_publish(mosq, NULL, mqttTopic, strlen(jsonString), jsonString,
                                      1, false);
                    free(jsonString);
                    cJSON_Delete(root);
                }
            }

            if (msgData.receivedVoltage) {
                char mqttTopic[strlen(MQTT_TOPIC_SENT_VOLTAGE_REPORT) 
                    + MQTT_TOPIC_MAX_SENSOR_LENGTH 
                    + 5 + 1];

                log4c_category_notice(clientlog, "SensorId=%d Battery Voltage %s", 
                                      msgData.sensorId, 
                                      msgData.voltageData);

                snprintf(mqttTopic, sizeof(mqttTopic), "%s/%d", 
                         MQTT_TOPIC_SENT_VOLTAGE_REPORT, msgData.sensorId);

                mosquitto_publish(mosq, NULL, mqttTopic, strlen(msgData.voltageData), 
                                  msgData.voltageData, 1, false);
            }

            // clear all the flags from the message data
            memset(&msgData, 0, sizeof(msgData));
        }
			
        usleep(5000);
	}
	bcm2835_spi_end();
	return 0;
}


/* vim: set cindent sw=4 ts=4 expandtab path+=/usr/local/include : */
