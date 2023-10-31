/*
 * obd2.c
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */
#include "main.h"
#include "obd2.h"
#include "obd2pids.h"
#include "bluetooth.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define OBD_HAL_TIMEOUT_UNIT1MS(x)	(x)

typedef enum {
	OBD_CONFIG_START, OBD_CONFIG_FINISH, OBD_CONFIG_TIMEOUT
} obdConfigState_e;

obdConfigState_e m_obdConfigState = OBD_CONFIG_START;
uint32_t m_obdSystickCnt;
uint8_t m_obdReConnectedFlag = 0;
uint8_t m_obdUartRecvCompleted = 0;

uint8_t m_obdUartBuffCnt;
uint8_t m_obdUartBuf[128];
char protocolType[3] = "A7";

void changeProtocolType(char *type) {
	memcpy(&protocolType[0], type, 2);
}

void setObdConfigState(obdConfigState_e state) {
	m_obdConfigState = state;
}

uint8_t getObdConfigState(void) {
	return (uint8_t) m_obdConfigState;
}

uint32_t getOBDSystick(void) {
	return m_obdSystickCnt;
}

uint8_t getOBDRecvCompleted(void) {
	return m_obdUartRecvCompleted;
}
void setOBDRecvCompleted(uint8_t state) {
	m_obdUartRecvCompleted = state;
}

uint8_t findOBDATCommandResp(char *resp) {
	return strstr((char*) m_obdUartBuf, resp) != NULL;
}

void clearOBDRxRawBuffCnt(void) {
	m_obdUartBuffCnt = 0;
}

uint8_t sendOBDUartData(uint8_t *data, uint8_t len) {
	clearOBDRxRawBuffCnt();
	return sendBtUartData(data, len);
}

void obd2Init(void) {
	typedef enum {
		OBD_ATZ,
		OBD_ATZ_WAIT,
		OBD_ECHO_OFF,
		OBD_ECHO_WAIT,
		OBD_SPACE_ON,
		OBD_SPACE_WAIT,
		OBD_SET_PROTOCOL,
		OBD_SET_PROTOCOL_WAIT,
		OBD_GET_PROTOCOL,
		OBD_GET_PROTOCOL_WAIT,
		EXIT
	} obdConfig_e;

	static obdConfig_e state = OBD_ATZ;
	static uint32_t timeout = 0;
	static uint8_t retry = 0;
	char setProtocolStr[20] = { 0 };
	m_obdConfigState = OBD_CONFIG_START;

	// bt connection and state pin check
	if (getBtConnState() != CONNECTED || getBTStatePinState() != GPIO_PIN_SET) {
		timeout = 0;
		retry = 0;
		state = OBD_ATZ;
		return;
	}

	if (getBTModePin() != GPIO_PIN_RESET) {
		setBTModePin(GPIO_PIN_RESET);
	}

	if (timeout == 0) {
		timeout = getOBDSystick();
	}

	switch (state) {
	case OBD_ATZ: {
		if (!sendOBDUartData((uint8_t*) "ATZ\r\n", sizeof((uint8_t*) "ATZ\r\n"))) {
			timeout = getOBDSystick();
			state = OBD_ATZ_WAIT;
		}
		break;
	}
	case OBD_ATZ_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("ELM327")) {
				state = OBD_ECHO_OFF;
			}
			else {
				state = OBD_ATZ;
			}
		}
		break;
	}
	case OBD_ECHO_OFF: {
		if (!sendOBDUartData((uint8_t*) "ATE0\r\n", sizeof((uint8_t*) "ATE0\r\n"))) {
			timeout = getOBDSystick();
			state = OBD_ECHO_WAIT;
		}
		break;
	}
	case OBD_ECHO_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("OK")) {
				state = OBD_SPACE_ON;
			}
		}
		break;
	}
	case OBD_SPACE_ON: {
		if (!sendOBDUartData((uint8_t*) "ATS1\r\n", sizeof((uint8_t*) "ATS1\r\n"))) {
			timeout = getOBDSystick();
			state = OBD_SPACE_WAIT;
		}
		break;
	}
	case OBD_SPACE_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("OK")) {
				state = OBD_SET_PROTOCOL;
			}
		}
		break;
	}
	case OBD_SET_PROTOCOL: {
		sprintf(&setProtocolStr[0], "ATSP%s", protocolType);
		if (!sendOBDUartData((uint8_t*) setProtocolStr, sizeof((uint8_t*) setProtocolStr))) {
			timeout = getOBDSystick();
			state = OBD_SET_PROTOCOL_WAIT;
		}
		break;
	}
	case OBD_SET_PROTOCOL_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("OK")) {
				state = OBD_GET_PROTOCOL;
			}
		}
		break;
	}
	case OBD_GET_PROTOCOL: {
		if (!sendOBDUartData((uint8_t*) "ATDPN\r\n", sizeof((uint8_t*) "ATDPN\r\n"))) {
			timeout = getOBDSystick();
			state = OBD_SET_PROTOCOL_WAIT;
		}
		break;
	}
	case OBD_GET_PROTOCOL_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp(protocolType)) {
				state = EXIT;
			}
		}
		break;
	}
	case EXIT:
	default:
		timeout = 0;
		retry = 0;
		state = OBD_ATZ;
		m_obdConfigState = OBD_CONFIG_FINISH;
	break;
	}

	if (getOBDSystick() - timeout > OBD_HAL_TIMEOUT_UNIT1MS(300) && retry < 3) {

		timeout = 0;
		state = OBD_ATZ;
		retry++;
	}
	else if (retry >= 3) {

		timeout = 0;
		retry = 0;
		state = OBD_ATZ;
		m_obdConfigState = OBD_CONFIG_TIMEOUT;
	}

}

void obd2Control(void) {
	if (!getBtConnState()) {
		if (getObdConfigState() == OBD_CONFIG_FINISH) {
			m_obdReConnectedFlag = 1;
		}
		return;
	}

	if (m_obdReConnectedFlag || (getObdConfigState() != OBD_CONFIG_FINISH)) {
		m_obdReConnectedFlag = 0;
		obd2Init();
	}

	if(getObdConfigState() == OBD_CONFIG_FINISH){
		//TODO: burada obd2 den datalar sorularak alınacak.
	}
}

void OBD_Virtual_Systick_Handler(void) {
	m_obdSystickCnt++;
}

void OBD_Virtual_Rx_Completed_Callback(unsigned char rxData) {
	m_obdUartBuf[m_obdUartBuffCnt++] = rxData;
	if (rxData == '>') {
		setOBDRecvCompleted(1);
	}
}

