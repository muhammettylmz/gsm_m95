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
#include <stdlib.h>

#define OBD_HAL_TIMEOUT_UNIT1MS(x)					(x)

static const uint8_t obd2ModeValidResponseDataList[OBD2_MODE_VALID_RESPONSE_DATA_SIZE] = { 0x41,
		0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x62, 0x61, 0x63 };

typedef enum {
	OBD_CONFIG_START, OBD_CONFIG_FINISH, OBD_CONFIG_TIMEOUT
} obdConfigState_e;

typedef enum {
	OBD2_PERIODIC_DATA_START, OBD2_PERIODIC_DATA_FINISH, OBD2_PERIODIC_DATA_TIMEOUT
} obd2GetPeriodicDataState_e;

obd2GetPeriodicDataState_e m_obd2GetPeriodicDataState = OBD2_PERIODIC_DATA_START;
obdConfigState_e m_obdConfigState = OBD_CONFIG_START;
uint32_t m_obdSystickCnt;
uint8_t m_obdReConnectedFlag = 0;
uint8_t m_obdUartRecvCompleted = 0;

uint8_t m_obdUartBuffCnt;
uint8_t m_obdUartBuf[128];
char protocolType[3] = "A7";  // auto and can 29bit/500kbps

const char *m_protocolTypeList[OBD2_PROTOCOL_TYPE_SIZE] = { "A1", "A2", "A3", "A4", "A5", "A6",
		"A7", "A8", "A9" };
uint8_t m_protocolTypeListIndex = 0;

char *obd2DataIDs[OBD2_PIDs_SIZE] = { "0100", "0101", "0102", "0103", "0104" };
uint8_t obd2DataIDsIndex = 0;

uint32_t m_obd2DataTimeout = 0;

uint32_t getOBDSystick(void) {
	return m_obdSystickCnt;
}

void startObd2DataTimeout(void) {
	m_obd2DataTimeout = getOBDSystick();
}

void stopObd2DataTimeout(void) {
	m_obd2DataTimeout = 0;
}

void changeProtocolType(char *type) {
	memcpy(&protocolType[0], type, 2);
}

void setObd2GetPeriodicDataState(obd2GetPeriodicDataState_e state) {
	m_obd2GetPeriodicDataState = state;
}
obd2GetPeriodicDataState_e getObd2GetPeriodicDataState(void) {
	return m_obd2GetPeriodicDataState;
}

void setObdConfigState(obdConfigState_e state) {
	m_obdConfigState = state;
}

uint8_t getObdConfigState(void) {
	return (uint8_t) m_obdConfigState;
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
	memset(m_obdUartBuf, 0, sizeof(m_obdUartBuf));
	setOBDRecvCompleted(0);
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
		OBD_SPACE_OFF,
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
				state = OBD_SPACE_OFF;
			}
		}
		break;
	}
	case OBD_SPACE_OFF: {
		if (!sendOBDUartData((uint8_t*) "ATS0\r\n", sizeof((uint8_t*) "ATS0\r\n"))) {
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
		sprintf(&setProtocolStr[0], "ATSP%s\r\n", protocolType);
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

/**
 * return
 * 0 is completed,
 * 1 is in progress,
 * 2 is timeout
 * */
uint8_t changeOBD2Protocol(const char *protocol) {
	typedef enum {
		OBD_SET_PROTOCOL, OBD_SET_PROTOCOL_WAIT, OBD_GET_PROTOCOL, OBD_GET_PROTOCOL_WAIT, EXIT
	} obdChangeProtocol_e;
	static obdChangeProtocol_e state = OBD_SET_PROTOCOL;
	static uint32_t timeout = 0;
	static uint8_t retry = 0;
	char setProtocolStr[30] = { 0 };

	if (timeout == 0) {
		timeout = getOBDSystick();
	}

	switch (state) {
	case OBD_SET_PROTOCOL: {
		sprintf(&setProtocolStr[0], "ATSP%s\r\n", protocol);
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
		return 0;
	break;
	}

	if (getOBDSystick() - timeout > OBD_HAL_TIMEOUT_UNIT1MS(300) && retry < 3) {
		timeout = 0;
		state = OBD_SET_PROTOCOL;
		retry++;
	}
	else if (retry >= 3) {
		timeout = 0;
		retry = 0;
		state = OBD_SET_PROTOCOL;
		return 2;
	}

	return 1;
}

uint8_t findArrayValue(uint8_t replyCmdId) {
	for (int i = 0; i < OBD2_MODE_VALID_RESPONSE_DATA_SIZE; ++i) {
		if (respObdCmd == obd2ModeValidResponseDataList[i]) {
			return 1;
		}
	}
	return 0;
}

void convertObd2Data(uint8_t pids) {
	//TODO: burada obd2uart bufdan alınan mesaj çözülerek ilgil pid e ye göre çevirme işlemi yapılacak.
	// ornek 41012233\r>
#define HEX_BASE_VALUE 16
	char *token = NULL;
	char strReplyPids[3] = { 0 };
	char strReplyObdCmd[3] = { 0 };
	uint8_t obd2Pid = 0;
	uint8_t respObdCmd = 0;
	uint32_t obdData = 0;

	token = strtok((char*) m_obdUartBuf, "\r");

	memcpy(strReplyObdCmd, &token[0], 2);
	respObdCmd = (uint8_t) strtoul(strReplyObdCmd, NULL, HEX_BASE_VALUE);

	if (!findArrayValue(respObdCmd)) {
		return;
	}

	if (respObdCmd == 0x43) {
		//TODO: DTC çevirme işlemi burada yapılacak.
		// convertDTCData();
	}
	else {

		memcpy(strReplyPids, &token[2], 2);
		obd2Pid = (uint8_t) strtol(strReplyPids, NULL, HEX_BASE_VALUE);

		switch (obd2Pid) {
		case 1:
		break;
		case 2:
		break;
		case 4:
		break;
		default:
		break;
		}
	}

}

void obd2GetPeriodicMsg(void) {
	typedef enum {
		OBD2_GET_PIDs_DATA, OBD2_GET_PIDs_DATA_WAIT, EXIT
	} obd2PeriodicMsg_e;

	static obd2PeriodicMsg_e state = OBD2_GET_PIDs_DATA;
	static uint32_t timeout = 0;
	static uint8_t retry = 0;
	static uint8_t noDataCount = 0;
	char strPidsNumber[8] = { 0 };

	setObd2GetPeriodicDataState(OBD2_PERIODIC_DATA_START);
	if (timeout == 0) {
		timeout = getOBDSystick();
	}
	switch (state) {
	case OBD2_GET_PIDs_DATA: {
		sprintf(&strPidsNumber[0], "%s\r\n", obd2DataIDs[obd2DataIDsIndex++]);
		if (!sendOBDUartData((uint8_t*) strPidsNumber, strlen(strPidsNumber))) {
			timeout = getOBDSystick();
			state = OBD2_GET_PIDs_DATA_WAIT;
		}
		break;
	}
	case OBD2_GET_PIDs_DATA_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("NODATA")) {
				if (obd2DataIDsIndex < OBD2_PIDs_SIZE) {
					state = OBD2_GET_PIDs_DATA;
					noDataCount++;
				}
				else if (noDataCount >= OBD2_PIDs_SIZE) {
					state = EXIT;
					obd2DataIDsIndex = 0;
				}
			}
			else {
				noDataCount = 0;
				if (obd2DataIDsIndex >= OBD2_PIDs_SIZE) {
					obd2DataIDsIndex = 0;
				}
				convertObd2Data();
				state = OBD2_GET_PIDs_DATA;
			}
		}
		break;
	}
	case EXIT:
	default:
		timeout = 0;
		noDataCount = 0;
		obd2DataIDsIndex = 0;
		retry = 0;
		state = OBD2_GET_PIDs_DATA;
		setObd2GetPeriodicDataState(OBD2_PERIODIC_DATA_TIMEOUT);
	break;
	}

	if (getOBDSystick() - timeout >= OBD_HAL_TIMEOUT_UNIT1MS(200)) {
		retry++;
		timeout = 0;
	}
	else if (retry >= 3) {
		state = EXIT;
	}
}

void obd2Control(void) {
	if (!getBtConnState()) {
		if (!m_obdReConnectedFlag && getObdConfigState() == OBD_CONFIG_FINISH) {
			m_obdReConnectedFlag = 1;
			setObdConfigState(OBD_CONFIG_START);
		}
		return;
	}

	if (m_obdReConnectedFlag || (getObdConfigState() != OBD_CONFIG_FINISH)) {
		m_obdReConnectedFlag = 0;
		obd2Init();
	}

	if (getObdConfigState() == OBD_CONFIG_FINISH) {
		//TODO: burada obd2 den datalar sorularak alınacak.
		//TODO: OBD2 pid sorgusunda NODATA veya hatalı bir durumolursa protocol değiştirip
		// changeOBD2Protocol fonksiyonu kullanılacak;
		/*//örnek kullanım
		 while(retVal != 2 ){
		 retVal = changeOBD2Protocol(tryProtocol);
		 if(retVal == 0){
		 break;
		 }
		 }
		 */
		if (getObd2GetPeriodicDataState() == OBD2_PERIODIC_DATA_TIMEOUT) {
			if (m_protocolTypeListIndex >= OBD2_PROTOCOL_TYPE_SIZE) {
				m_protocolTypeListIndex = 0;
			}
			if (changeOBD2Protocol(m_protocolTypeList[m_protocolTypeListIndex++]) == 0) {
				setObd2GetPeriodicDataState(OBD2_PERIODIC_DATA_START);
			}
		}
		else {
			obd2GetPeriodicMsg();
		}
	}
}

void OBD_Virtual_Systick_Handler(void) {
	m_obdSystickCnt++;
	if (getOBDSystick() - m_obd2DataTimeout >= OBD_HAL_TIMEOUT_UNIT1MS(300)) {
		setOBDRecvCompleted(1);
		stopObd2DataTimeout();
	}
}

void OBD_Virtual_Rx_Completed_Callback(unsigned char rxData) {
	m_obdUartBuf[m_obdUartBuffCnt++] = rxData;
	startObd2DataTimeout();
	if (rxData == '>') {
		setOBDRecvCompleted(1);
		stopObd2DataTimeout();
	}

	if (m_obdUartBuffCnt >= 128) {
		m_obdUartBuffCnt = 0;
	}
}

