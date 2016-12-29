#ifndef DEV_HRF_H
#define DEV_HRF_H

#include <stdint.h>

#define SEED_PID			0x01
#define MANUF_SENTEC        0x01
#define PRODUCT_SENTEC_DEFAULT 0x01
#define MESSAGE_BUF_SIZE	66
#define MAX_FIFO_SIZE		66
#define TRUE				1
#define FALSE				0

#define ADDR_FIFO			0x00
#define ADDR_OPMODE			0x01	// Operating modes
#define ADDR_REGDATAMODUL	0x02
#define ADDR_FDEVMSB		0x05
#define ADDR_FDEVLSB		0x06
#define ADDR_FRMSB			0x07
#define ADDR_FRMID			0x08
#define ADDR_FRLSB			0x09
#define ADDR_AFCCTRL		0x0B
#define ADDR_LNA			0x18
#define ADDR_RXBW			0x19
#define ADDR_AFCFEI			0x1E
#define ADDR_IRQFLAGS1		0x27
#define ADDR_IRQFLAGS2		0x28
#define ADDR_RSSITHRESH		0x29
#define ADDR_PREAMBLELSB	0x2D
#define ADDR_SYNCCONFIG		0x2E
#define ADDR_SYNCVALUE1		0x2F
#define ADDR_SYNCVALUE2		0X30
#define ADDR_SYNCVALUE3		0x31
#define ADDR_SYNCVALUE4		0X32
#define ADDR_PACKETCONFIG1	0X37
#define ADDR_PAYLOADLEN		0X38
#define ADDR_NODEADDRESS	0X39
#define ADDR_FIFOTHRESH		0X3C

#define MASK_REGDATAMODUL_OOK	0x08
#define MASK_REGDATAMODUL_FSK	0x00
#define MASK_WRITE_DATA		0x80
#define MASK_MODEREADY		0x80
#define MASK_FIFONOTEMPTY	0x40
#define MASK_FIFOLEVEL		0x20
#define MASK_FIFOOVERRUN	0x10
#define MASK_PACKETSENT		0x08
#define MASK_TXREADY		0x20
#define MASK_PACKETMODE		0x60
#define MASK_MODULATION		0x18
#define MASK_PAYLOADRDY		0x04

/* Precise register description can be found on: 
 * www.hoperf.com/upload/rf/RFM69W-V1.3.pdf
 * on page 63 - 74
 */
#define MODE_STANDBY 			0x04	// Standby
#define MODE_TRANSMITER 		0x0C	// Transmiter
#define MODE_RECEIVER 			0x10	// Receiver
#define VAL_REGDATAMODUL_FSK	0x00	// Modulation scheme FSK
#define VAL_REGDATAMODUL_OOK	0x08	// Modulation scheme OOK
#define VAL_FDEVMSB30			0x01	// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
#define VAL_FDEVLSB30			0xEC	// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
#define VAL_FRMSB434			0x6C	// carrier freq -> 434.3MHz 0x6C9333
#define VAL_FRMID434			0x93	// carrier freq -> 434.3MHz 0x6C9333
#define VAL_FRLSB434			0x33	// carrier freq -> 434.3MHz 0x6C9333
#define VAL_FRMSB433			0x6C	// carrier freq -> 433.92MHz 0x6C7AE1
#define VAL_FRMID433			0x7A	// carrier freq -> 433.92MHz 0x6C7AE1
#define VAL_FRLSB433			0xE1	// carrier freq -> 433.92MHz 0x6C7AE1
#define VAL_AFCCTRLS			0x00	// standard AFC routine
#define VAL_AFCCTRLI			0x20	// improved AFC routine
#define VAL_LNA50				0x08	// LNA input impedance 50 ohms
#define VAL_LNA50G				0x0E	// LNA input impedance 50 ohms, LNA gain -> 48db
#define VAL_LNA200				0x88	// LNA input impedance 200 ohms
#define VAL_RXBW60				0x43	// channel filter bandwidth 10kHz -> 60kHz  page:26
#define VAL_RXBW120				0x41	// channel filter bandwidth 120kHz
#define VAL_AFCFEIRX			0x04	// AFC is performed each time RX mode is entered
#define VAL_RSSITHRESH220		0xDC	// RSSI threshold 0xE4 -> 0xDC (220)
#define VAL_PREAMBLELSB3		0x03	// preamble size LSB 3
#define VAL_PREAMBLELSB5		0x05	// preamble size LSB 5
#define VAL_SYNCCONFIG2			0x88	// Size of the Synch word = 2 (SyncSize + 1)
#define VAL_SYNCCONFIG4			0x98	// Size of the Synch word = 4 (SyncSize + 1)
#define VAL_SYNCVALUE1FSK		0x2D	// 1st byte of Sync word
#define VAL_SYNCVALUE2FSK		0xD4	// 2nd byte of Sync word
#define VAL_SYNCVALUE1OOK		0x80	// 1nd byte of Sync word
#define VAL_PACKETCONFIG1FSK	0xA2	// Variable length, Manchester coding, Addr must match NodeAddress
#define VAL_PACKETCONFIG1OOK	0		// Fixed length, no Manchester coding
#define VAL_PAYLOADLEN255		0xFF	// max Length in RX, not used in Tx
#define VAL_PAYLOADLEN64		0x40	// max Length in RX, not used in Tx
#define VAL_PAYLOADLEN_OOK		(13 + 8 * 17)	// Payload Length
#define VAL_NODEADDRESS01		0x04	// Node address used in address filtering
#define VAL_FIFOTHRESH1			0x81	// Condition to start packet transmission: at least one byte in FIFO
#define VAL_FIFOTHRESH30		0x1E	// Condition to start packet transmission: wait for 30 bytes in FIFO

#define MSG_REMAINING_LEN 0
#define MSG_MANUF_ID      1
#define MSG_PRODUCT_ID    2
#define MSG_RESERVED_HI   3
#define MSG_RESERVED_LO   4
#define MSG_SENSOR_ID_2   5
#define MSG_SENSOR_ID_1   6
#define MSG_SENSOR_ID_0   7
#define MSG_DATA_START    8
#define MSG_ENCR_START    MSG_SENSOR_ID_2
#define MSG_OVERHEAD_LEN  (MSG_DATA_START+2)

#define MAX_DATA_LENGTH MESSAGE_BUF_SIZE

/* OOK Message Parameters */
#define OOK_BUF_SIZE 17
#define OOK_MSG_ADDRESS_LENGTH  10   /* 10 bytes in address */


enum ledColor {
    redLED = RPI_V2_GPIO_P1_15,
    greenLED = RPI_V2_GPIO_P1_13
};

#define RESET_PIN RPI_V2_GPIO_P1_22

enum ledOnOff {
    ledOff = LOW,
    ledOn = HIGH
};

void ledControl(enum ledColor led, enum ledOnOff OnOff);

typedef struct regSet_t {
	uint8_t addr;
	uint8_t val;
} regSet_t;

typedef enum {
	S_MSGLEN = 1,
	S_MANUFID,
	S_PRODID,
	S_ENCRYPTPIP,
	S_SENSORID,
	S_DATA_PARAMID,
	S_DATA_TYPEDESC,
	S_DATA_VAL,
	S_CRC,
	S_FINISH
} state_t;

typedef struct msg_t {
	state_t state;
	uint8_t msgSize;
	uint8_t recordBytesToRead;
	uint8_t manufId;
	uint8_t prodId;
	uint32_t sensorId;
	uint8_t	gotJoin;
	uint8_t paramId;
	uint8_t type;
	uint16_t pip;
	uint32_t value;
	uint8_t bufCnt;
	uint8_t buf[MESSAGE_BUF_SIZE];
        uint8_t crcPassed;
} msg_t;

struct ReceivedMsgData {
    uint8_t msgAvailable;
    uint8_t joinCommand;
    uint8_t manufId;
    uint8_t prodId;
    uint32_t sensorId;
    uint8_t receivedTempReport;
    char    receivedTemperature[MAX_DATA_LENGTH];
    uint8_t receivedDiagnostics;
    uint8_t diagnosticData[2];   /* 0 - Low Byte, 1 - High Byte */
    uint8_t receivedVoltage;
    char voltageData[MAX_DATA_LENGTH];
};



void 	HRF_config_FSK();
void 	HRF_config_OOK();
void 	HRF_clr_fifo(void);
void 	HRF_reg_Rn(uint8_t* , uint8_t, uint8_t);
void 	HRF_reg_Wn(uint8_t*, uint8_t, uint8_t);
uint8_t HRF_reg_R(uint8_t);
void 	HRF_reg_W(uint8_t, uint8_t);
void 	HRF_change_mode(uint8_t);
void 	HRF_assert_reg_val(uint8_t, uint8_t, uint8_t, char*);
void 	HRF_wait_for(uint8_t, uint8_t, uint8_t);
void	HRF_send_OOK_msg(uint8_t *address, int socketNum, int On, int repeat);
uint8_t* HRF_make_FSK_msg(uint8_t, uint8_t, uint8_t, uint32_t, uint8_t, ...);
void 	HRF_send_FSK_msg(uint8_t*, uint8_t);
//void 	decryptMsg(uint8_t*, uint8_t);
void 	encryptMsg(uint8_t, uint8_t*, uint8_t);
void 	setupCrc(uint8_t*);
void 	HRF_receive_FSK_msg(uint8_t, uint8_t, uint8_t, struct ReceivedMsgData *);
void 	msgNextState(uint8_t, uint8_t, uint8_t, msg_t*, struct ReceivedMsgData *);
char* 	getIdName(uint8_t);
char* 	getValString(uint64_t, uint8_t, uint8_t);



//#define manufacturerId 0x04
//#define product_id     0x03

//const uint8_t sensor_id[3] = { 0xff, 0xff, 0xff };

#endif /* DEV_HRF_H */

/* vim: set ts=4 sw=4 */
