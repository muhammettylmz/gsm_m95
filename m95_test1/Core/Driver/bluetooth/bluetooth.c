/*
 * bluetooth.c
 *
 *  Created on: Aug 19, 2023
 *      Author: muham
 */

#include "bluetooth.h"
#include "obd2.h"
#include "main.h"
#include "uart_debug.h"
#include "m95.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BT_AT_LINK_FMT		(const char*)"AT+LINK=%s\r\n"

#define HAL_TIMEOUT_UNIT1MS(x)	(x)

#define BT_UART_BUFF_ARR_SIZE	128
#define BT_UART_BUFF_SIZE		50
#define BT_UART_TIMEOUT  		5

typedef enum {
	BT_CONFIG_START, BT_CONFIG_FINISH, BT_CONFIG_TIMEOUT
} btConfigState_e;
typedef enum {
	BT_CONNECTION_START, BT_CONNECTION_FINISH, BT_CONNECTION_TIMEOUT
} btConnectionState_e;

typedef enum {
	BT_RECV_COMPLETED_IDLE, BT_RECV_COMPLETED
} btRecvComp_e;

btConfigState_e m_btConfigState = BT_CONFIG_START;
btConnectionState_e m_btConnectionState = BT_CONNECTION_START;

connState_e m_btSerialConnState = DISCONNECTED;
connState_e m_btConnState = DISCONNECTED;
connState_e m_prevBtSerialConnState = DISCONNECTED;

UART_HandleTypeDef *m_btUart;
HAL_StatusTypeDef m_btRxIterror;

uint32_t m_btSystick;
uint32_t m_prevBtRxTimeoutCnt;
btRecvComp_e m_btRecvCompleted = BT_RECV_COMPLETED_IDLE;

uint8_t btRxRawBuff[255];
uint8_t btRxData;
uint8_t btRxRawBuffCnt;

char m_obd2MacAddrForHc05[15];

GPIO_PinState m_btModePinState = GPIO_PIN_RESET;
GPIO_PinState m_btStatePinState = GPIO_PIN_RESET;

uint8_t getBTStatePinState(void) {
	return (uint8_t) m_btStatePinState;
}

void setBTModePin(uint8_t state) {
	HAL_GPIO_WritePin(BT_MODE_PIN_GPIO_Port, BT_MODE_PIN_Pin, (GPIO_PinState) state);
	m_btModePinState = (GPIO_PinState) state;
}

uint8_t getBTModePin(void) {
	return (uint8_t) m_btModePinState;
}

uint8_t getBtConfigState(void) {
	return (uint8_t) m_btConfigState;
}

void setBtConfigState(btConfigState_e state) {
	m_btConfigState = state;
}

uint8_t getBtConnectionState(void) {
	return (uint8_t) m_btConnectionState;
}

void setBtConnectionState(btConnectionState_e state) {
	m_btConnectionState = state;
}

uint8_t getBtSerialConnState(void) {
	return (uint8_t) m_btSerialConnState;
}

void setBtSerialConnState(uint8_t state) {
	m_btSerialConnState = (connState_e) state;
}

uint32_t getBTSystick(void) {
	return m_btSystick;
}

uint8_t getBTRecvCompleted(void) {
	return (uint8_t) m_btRecvCompleted;
}

void setRecvCompleted(btRecvComp_e state) {
	m_btRecvCompleted = state;
}

void stopBTRxTimeout(void) {
	m_prevBtRxTimeoutCnt = 0;
}

void startBTRxTimeout(void) {
	m_prevBtRxTimeoutCnt = getBTSystick();
	setRecvCompleted(BT_RECV_COMPLETED_IDLE);
}

void checkBTRxTimeoutCompleted(void) {
	if ((getBTSystick() - m_prevBtRxTimeoutCnt) >= HAL_TIMEOUT_UNIT1MS(BT_UART_TIMEOUT)) {
		setRecvCompleted(BT_RECV_COMPLETED);
		stopBTRxTimeout();
	}
}

void clearBtRxRawBuffCnt(void) {

//	for (uint8_t u8 = 0; u8 < btRxRawBuffCnt; u8++) {
//		btRxRawBuff[u8] = 0;
//	}
	memset(btRxRawBuff, 0, sizeof(btRxRawBuff));
	setRecvCompleted(BT_RECV_COMPLETED_IDLE);
	btRxRawBuffCnt = 0;
	//memcpy(btRxRawBuff, 0 , 64);
}

void clearBtStateFlags(void) {
	m_btConfigState = BT_CONFIG_START;
	m_btConnState = DISCONNECTED;
	m_btSerialConnState = DISCONNECTED;
	//setObdConfigState(OBD_CONFIG_START);
}

uint8_t findBTATCommandResp(char *resp) {
	return strstr((char*) btRxRawBuff, resp) != NULL;
}

uint8_t sendBtUartData(uint8_t *data, uint8_t len) {
	clearBtRxRawBuffCnt();
	if ((HAL_UART_Transmit(m_btUart, data, len, 50)) != HAL_OK) {
		return 1;
	}
	return 0;
}

uint8_t getBtConnState(void) {
	return (uint8_t) m_btConnState;
}

void btInitConfig(void) {
	//TODO: HM-10 Bluetooth konfigürasyon AT ayarları yapılacak.(GSM gibi yap)
	typedef enum {
		BT_AT_SEND, BT_AT_SEND_WAIT, BT_ROLE_MODE, BT_ROLE_MODE_WAIT, EXIT
	} btConfigState_e;

	static btConfigState_e initState = BT_AT_SEND;
	static uint8_t retry = 0;
	static uint32_t timeout = 0;
	uint32_t waitResponseTimeout = 300;

	if (timeout == 0) {
		timeout = getBTSystick();
	}
	m_btConfigState = BT_CONFIG_START;

	switch (initState) {
	case BT_AT_SEND:
		if (!sendBtUartData((uint8_t*) "AT\r\n", sizeof((uint8_t*) "AT\r\n"))) {
			timeout = getBTSystick();
			initState = BT_AT_SEND_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_AT_SEND_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK")) {
				initState = BT_ROLE_MODE;
			}
		}
	break;
	case BT_ROLE_MODE:
		if (!sendBtUartData((uint8_t*) "AT+ROLE=1\r\n", 11)) {  //sizeof((uint8_t*) "AT+ROLE=1\r\n"))) {
			timeout = getBTSystick();
			initState = BT_ROLE_MODE_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_ROLE_MODE_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK")) {
				//initState = BT_MODE;
				initState = EXIT;
			}
		}
	break;
	case EXIT:
	default:
		m_btSerialConnState = CONNECTED;
		timeout = 0;
		retry = 0;
		initState = BT_AT_SEND;
		m_btConfigState = BT_CONFIG_FINISH;
	break;
	}

//	if (getBTRecvCompleted()) {
//		if (findBTATCommandResp("OK")) {
//			initState++;
//		}
//	}

	if (getBTSystick() - timeout > HAL_TIMEOUT_UNIT1MS(waitResponseTimeout) && retry < 3) {
		//m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		initState = BT_AT_SEND;
		retry++;
	}
	else if (retry >= 3) {
		m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		retry = 0;
		initState = BT_AT_SEND;
		m_btConfigState = BT_CONFIG_TIMEOUT;
	}

}
uint8_t findObd2Arr[255] = {0};
//	="+INQ:703E:97:E0332B,1A011C,FFAF,TEST\r\n+INQ:703E:97:E0332B,1A011C,FFAF,HAKO Pro\r\n+INQ:661E:32:2F1046,240404,FFC5,OBDII\r\n+INQ:703E:97:E0332B,1A011C,FFAF,TEST\r\n";
void findOBD2DeviceMacAddr(char *addr) {
	char *ftoken;
	char *obd2token = NULL;
	char *macs[3];
	char *tokenArr[5];
	uint8_t tokenIndex = 0;
	uint8_t indis = 0;
	memset(findObd2Arr , 0 , 255);
	memcpy(findObd2Arr, btRxRawBuff , btRxRawBuffCnt);

	// BT response +INQ:xxxx:yy:zzzzzz,bbbbb,cccc,name
	char *token;
	token = strtok((char*) findObd2Arr, "\r");
	//token = strtok((char*) test, "\n");
	while (token != NULL) {
		tokenArr[tokenIndex++] = token;
		token = strtok(NULL, "\r");
	}

	for (uint8_t u8 = 0; u8 < tokenIndex; u8++) {
		if (strstr(tokenArr[u8], "OBD") != NULL) {
			obd2token = tokenArr[u8];
			break;
		}
	}

	if (obd2token == NULL) {
		return;
	}

	//first token is +INQ:xxxx:xx:xxxxxx
	ftoken = strtok((char*) obd2token, ",");
	//second token +INQ
	ftoken = strtok((char*) ftoken, ":");
	//first mac token xxxx
	ftoken = strtok(NULL, ":");
	while (ftoken != NULL) {
		if (indis >= 3) {
			break;
		}
		macs[indis++] = ftoken;
		ftoken = strtok(NULL, ":");
	}
	sprintf(&addr[0], "%s,%s,%s", macs[0], macs[1], macs[2]);
}

void btConnection(void) {
#define BT_INQ_REPLY_TIMEOUT 25000
#define BT_DEFAULT_REPLY_TIMEOUT 10000
#define BT_REQ_STATE_TRY_CNT 5
	char atLinkCommad[30];
	typedef enum {
		BT_INQ, BT_INQ_WAIT, /*BT_BIND, BT_BIND_WAIT,*/
		BT_LINK, BT_LINK_WAIT, BT_STATE_REQ, BT_STATE_REP, EXIT
	} btConnState_e;

	static btConnState_e connState = BT_INQ;
	static uint8_t retry = 0;
	static uint32_t timeout = 0;
	static uint8_t reqStateTryCnt = 0;
	static uint32_t waitResponseTimeout = 300;

	if (timeout == 0) {
		timeout = getBTSystick();
	}
	m_btConnectionState = BT_CONNECTION_START;
	//BT_Virtual_Rx_IT();

	switch (connState) {
	case BT_INQ: {
		if (!sendBtUartData((uint8_t*) "AT+INQ\r\n", 8)) {  // sizeof((uint8_t*) "AT+INQ\r\n"))) {
			timeout = getBTSystick();
			waitResponseTimeout = BT_INQ_REPLY_TIMEOUT;
			connState = BT_INQ_WAIT;
		}
		break;
	}
	case BT_INQ_WAIT: {
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OBD")) {
				connState = BT_LINK;
			}
			else {
				if (findBTATCommandResp("OK")) {
					connState = BT_INQ;
				}
			}
		}
		break;
	}
	case BT_LINK: {
		findOBD2DeviceMacAddr(m_obd2MacAddrForHc05);
		sprintf(&atLinkCommad[0], BT_AT_LINK_FMT, m_obd2MacAddrForHc05);
		if (!sendBtUartData((uint8_t*) atLinkCommad, strlen(atLinkCommad))) {
			timeout = getBTSystick();
			waitResponseTimeout = BT_DEFAULT_REPLY_TIMEOUT;
			connState = BT_LINK_WAIT;
		}
		break;
	}
	case BT_LINK_WAIT: {
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK")) {
				connState = BT_STATE_REQ;
			}
		}
		break;
	}
	case BT_STATE_REQ: {
		reqStateTryCnt++;
		if (reqStateTryCnt > BT_REQ_STATE_TRY_CNT) {
			connState = BT_INQ;
			reqStateTryCnt = 0;
			break;
		}
		if (!sendBtUartData((uint8_t*) "AT+STATE?\r\n", 11)) {  // sizeof((uint8_t*) "AT+STATE?\r\n"))) {
			timeout = getBTSystick();
			waitResponseTimeout = BT_DEFAULT_REPLY_TIMEOUT;
			connState = BT_STATE_REP;
		}
		break;
	}
	case BT_STATE_REP: {
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("+STATE:CONNECTED")) {
				connState = EXIT;
				m_btConnState = CONNECTED;
			}
			else {
				connState = BT_STATE_REQ;
			}
		}
		break;
	}
	case EXIT:
	default:
		m_btConnectionState = BT_CONNECTION_FINISH;
		connState = BT_INQ;
		timeout = 0;
		retry = 0;
		reqStateTryCnt = 0;
	break;
	}

	if (getBTSystick() - timeout > HAL_TIMEOUT_UNIT1MS(waitResponseTimeout) && retry < 3) {
		m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		connState = BT_INQ;
		retry++;
	}
	else if (retry >= 3) {
		m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		retry = 0;
		connState = BT_INQ;
		m_btConnectionState = BT_CONNECTION_TIMEOUT;
	}
}

void checkBtConnState(void) {

	if (getBtConnState() == CONNECTED && m_btStatePinState == GPIO_PIN_SET) {
		return;
	}

	if (getBtSerialConnState() != CONNECTED) {
		setBTModePin(GPIO_PIN_SET);
		btInitConfig();
		return;
	}

	//TODO: bluetooth cihazı ile bağlantı işlemini burada yap.

}

void btSearchBTCommandResponse(void) {
	if (getBTRecvCompleted() == BT_RECV_COMPLETED_IDLE && getBtConnState() != CONNECTED) {
		return;
	}

	//TODO: burada bt connected, reset, gelen mesaj vb. gibi kontrol işlemleri yapılacak.
}

void btInit(void *uart) {
	m_btUart = (UART_HandleTypeDef*) uart;
	BT_Virtual_Rx_IT();
	setBTModePin(GPIO_PIN_RESET);
}

void btControl(void) {

	findOBD2DeviceMacAddr(NULL);

	if (m_btRxIterror != HAL_OK) {
		BT_Virtual_Rx_IT();
	}

	if (m_btConnState != CONNECTED && getBtConfigState() != BT_CONFIG_FINISH) {
		setBTModePin(GPIO_PIN_SET);
		btInitConfig();
		return;
	}

	if (m_btConnState != CONNECTED && getBtConfigState() == BT_CONFIG_FINISH
			&& getBtConnectionState() != BT_CONNECTION_FINISH) {
		setBTModePin(GPIO_PIN_SET);
		btConnection();
		return;
	}

	checkBtConnState();

}

void BT_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_btUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		btRxRawBuff[btRxRawBuffCnt++] = btRxData;
		BT_Virtual_Rx_IT();
		startBTRxTimeout();
//		if (btRxRawBuffCnt >= 128) {
//			btRxRawBuffCnt = 0;
//		}
		if (getBtConnState() == CONNECTED) {
			OBD_Virtual_Rx_Completed_Callback(btRxData);
		}
	}
}

void checkStatePin(GPIO_PinState *pin) {
	static uint16_t pinSetCnt = 0;
	static uint16_t pinResetCnt = 0;

	//debounce cnt
	if (HAL_GPIO_ReadPin(BT_STATE_PIN_GPIO_Port, BT_STATE_PIN_Pin) == GPIO_PIN_SET) {
		pinSetCnt++;
		pinResetCnt = 0;
	}
	else {
		pinSetCnt = 0;
		pinResetCnt++;
	}

	// state high
	if (pinSetCnt >= HAL_TIMEOUT_UNIT1MS(25)) {
		pinSetCnt = 0;
		*pin = GPIO_PIN_SET;
		m_btConnState = CONNECTED;
	}
	//state low
	else if (pinResetCnt >= HAL_TIMEOUT_UNIT1MS(25)) {
		pinResetCnt = 0;
		*pin = GPIO_PIN_RESET;
		if(m_btConnState == CONNECTED){
			clearBtStateFlags();
		}
		m_btConnState = DISCONNECTED;
	}
}

void BT_Virtual_Systick(void) {
	m_btSystick++;
	if (m_prevBtRxTimeoutCnt != 0) {
		checkBTRxTimeoutCompleted();
	}
	checkStatePin(&m_btStatePinState);

}

void BT_Virtual_Rx_IT(void) {
	m_btRxIterror = HAL_UART_Receive_IT(m_btUart, &btRxData, 1);
}

