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

#define OT_JOIN_RESP	0x6A
#define OT_JOIN_CMD		0xEA

#define OT_POWER		0x70
#define OT_REACTIVE_P	0x71

#define OT_CURRENT		0x69
#define OT_ACTUATE_SW	0xF3
#define OT_FREQUENCY	0x66
#define OT_TEST			0xAA
#define OT_SW_STATE		0x73

#define OT_TEMP_SET		0xf4    /* Send new target temperature to driver board */
#define OT_TEMP_REPORT	0x74    /* Send externally read room temperature to motor board */


#define OT_VOLTAGE		0x76

#define OT_EXERCISE_VALVE   0xA3    /* Send exercise valve command to driver board. 
                                       Read diagnostic flags returned by driver board. 
                                       Send diagnostic flag acknowledgement to driver board. 
                                       Report diagnostic flags to the gateway. 
                                       Flash red LED once every 5 seconds if ‘battery dead’ flag
                                       is set.
                                         Unsigned Integer Length 0
                                    */

#define OT_REQUEST_VOLTAGE 0xE2     /* Request battery voltage from driver board. 
                                       Report battery voltage to gateway. 
                                       Flash red LED 2 times every 5 seconds if voltage 
                                       is less than 2.4V
                                         Unsigned Integer Length 0
                                       */
#define OT_REPORT_VOLTAGE   0x62    /* Volts 
                                         Unsigned Integer Length 0
                                         */

#define OT_REQUEST_DIAGNOTICS 0xA6  /*   Read diagnostic flags from driver board and report 
                                         these to gateway Flash red LED once every 5 seconds 
                                         if ‘battery dead’ flag is set
                                         Unsigned Integer Length 0
                                         */

#define OT_REPORT_DIAGNOSTICS 0x26

#define OT_SET_VALVE_STATE 0xA5     /*
                                       Send a message to the driver board
                                       0 = Set Valve Fully Open
                                       1=Set Valve Fully Closed
                                       2 = Set Normal Operation
                                       Valve remains either fully open or fully closed until 
                                       valve state is set to ‘normal operation’.
                                       Red LED flashes continuously while motor is running
                                       terminated by three long green LED flashes when valve 
                                       fully open or three long red LED flashes when valve is 
                                       closed

                                       Unsigned Integer Length 1
                                       */

#define OT_SET_LOW_POWER_MODE 0xA4  /*
                                       0=Low power mode off
                                       1=Low power mode on

                                       Unsigned Integer Length 1
                                       */                                       
#define OT_IDENTIFY     0xBF

#define OT_SET_REPORTING_INTERVAL 0xD2 /*
                                          Update reporting interval to requested value

                                       Unsigned Integer Length 2
                                          */                                          

#define OT_CRC			0x00

#define SIZE_MSGLEN			1
#define SIZE_MANUF_ID       1
#define SIZE_PRODID			1
#define SIZE_ENCRYPTPIP		2
#define SIZE_SENSORID		3
#define SIZE_DATA_PARAMID	1
#define SIZE_DATA_TYPEDESC	1
#define SIZE_CRC			2


/* vim: set cindent sw=4 ts=4 expandtab path+=/usr/local/include : */
