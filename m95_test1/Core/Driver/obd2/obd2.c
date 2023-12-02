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
#include <stdbool.h>
#include <stdlib.h>

#define OBD_HAL_TIMEOUT_UNIT1MS(x)					(x)
#define OBD2_UART_RAW_DATA_SIZE 					128

static const uint8_t obd2ModeValidResponseDataList[OBD2_MODE_VALID_RESPONSE_DATA_SIZE] = { 0x41,
		0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x62, 0x61, 0x63 };

static const char strDTCs[16][3] = { "P0\0", "P1\0", "P2\0", "P3\0", "C0\0", "C1\0", "C2\0", "C3\0",
		"B0\0", "B1\0", "B2\0", "B3\0", "U0\0", "U1\0", "U2\0", "U3\0" };

typedef enum {
	OBD2_PERIODIC_DATA_START, OBD2_PERIODIC_DATA_FINISH, OBD2_PERIODIC_DATA_TIMEOUT
} obd2GetPeriodicDataState_e;

obd2GetPeriodicDataState_e m_obd2GetPeriodicDataState = OBD2_PERIODIC_DATA_START;
obdConfigState_e m_obdConfigState = OBD_CONFIG_START;
uint32_t m_obdSystickCnt;
uint8_t m_obdReConnectedFlag = 0;
uint8_t m_obdUartRecvCompleted = 0;
uint8_t m_obdUartBuffCnt;
uint8_t m_obdUartBuf[OBD2_UART_RAW_DATA_SIZE];
uint8_t m_changeProtocolFlag = 0;
char protocolType[3] = "A7";  // auto and can 29bit/500kbps

const char *m_protocolTypeList[OBD2_PROTOCOL_TYPE_SIZE] = { "A1", "A2", "A3", "A4", "A5", "A6",
		"A7", "A8", "A9" };
uint8_t m_protocolTypeListIndex = 0;

char *obd2DataIDs[OBD2_PIDs_SIZE] = { "AT IGN", "0104", "0105", "010A", "010C", "010D", "012F", "015C",
		"015E", "01A6", "03" };
uint8_t obd2DataIDsIndex = 0;

uint32_t m_obd2DataTimeout = 0;

obd2VehicleData_t obd2VehicleData;

obd2PeriodicDataCompletedState_e m_obd2PeriodicDataState = OBD2_PERIODIC_DATA_IDLE;

void setObd2PeroidicDataCompletedState(obd2PeriodicDataCompletedState_e state) {
	m_obd2PeriodicDataState = state;
}

obd2PeriodicDataCompletedState_e getObd2PeroidicDataCompletedState(void) {
	return m_obd2PeriodicDataState;
}

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
		if (!sendOBDUartData((uint8_t*) "ATZ\r", 4)) {  // sizeof((uint8_t*) "ATZ\r"))) {
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
		if (!sendOBDUartData((uint8_t*) "AT E0\r", 6)) {  // sizeof((uint8_t*) "ATE0\r"))) {
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
		if (!sendOBDUartData((uint8_t*) "AT S0\r", 6)) {  // sizeof((uint8_t*) "ATS0\r"))) {
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
		sprintf(&setProtocolStr[0], "AT SP%s\r", protocolType);
		if (!sendOBDUartData((uint8_t*) setProtocolStr, strlen(setProtocolStr))) {
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
		if (!sendOBDUartData((uint8_t*) "AT DPN\r", 7)) {  // sizeof((uint8_t*) "ATDPN\r"))) {
			timeout = getOBDSystick();
			state = OBD_GET_PROTOCOL_WAIT;
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
		setBtSerialConnState(1);
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
		setBtSerialConnState(0);
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
		sprintf(&setProtocolStr[0], "AT SP%s\r", protocol);
		if (!sendOBDUartData((uint8_t*) setProtocolStr, strlen(setProtocolStr))) {
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
		if (!sendOBDUartData((uint8_t*) "ATDPN\r", sizeof((uint8_t*) "ATDPN\r"))) {
			timeout = getOBDSystick();
			state = OBD_GET_PROTOCOL_WAIT;
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
		if (replyCmdId == obd2ModeValidResponseDataList[i]) {
			return 1;
		}
	}
	return 0;
}

obd2VehicleData_t getPeriodicObdVehicleData(void) {
	return obd2VehicleData;
}

bool convertObd2Data(void) {
#define HEX_BASE_VALUE 				16
#define OBD_RESP_CMD_IDx 			0
#define OBD_RESP_PID_IDx 			2
#define OBD_RESP_DATA_IDx 			4
#define FIRST_DTC_DATA_INDEX		2
#define DTC_STR_DATA_SIZE			4
#define DTC_RECORD_SIZE 			1
#define DTC_STR_DATA_VALUE_SIZE 	3
#define FIRST_DTC_STR_DATA_INDEX 	(FIRST_DTC_DATA_INDEX+1)
#define OBD2_MIN_RESPONSE_SIZE 		4
#define OBD2_SERVICE_MODE_3_RESPONSE 0x43
#define OBD2_REPLY_CMD_ID_STR_SIZE 2
#define OBD2_REPLY_PIDs_STR_SIZE 2

	char *token = NULL;
	char *tokenArr[5];
	uint8_t tokenArrIndex = 0;
	char strReplyPids[3] = { 0 };
	char strReplyObdCmd[3] = { 0 };
	uint8_t obd2Pid;
	uint8_t respObdCmd;
	uint32_t obd2Data;

	token = strtok((char*) m_obdUartBuf, "\r");

	while (token != NULL) {
		tokenArr[tokenArrIndex++] = token;
		token = strtok(NULL, "\r");
	}

	if (tokenArrIndex) {
		tokenArrIndex--;
	}

	if (tokenArrIndex == 0) {
		return false;
	}
	else {
		memcpy(strReplyObdCmd, &tokenArr[0][OBD_RESP_CMD_IDx], OBD2_REPLY_CMD_ID_STR_SIZE);
		// array 1 de > karakteri mevcut
	}

	respObdCmd = (uint8_t) strtoul(strReplyObdCmd, NULL, HEX_BASE_VALUE);
	if (!findArrayValue(respObdCmd)) {
		return false;
	}

	uint8_t size = strlen(tokenArr[0]);
	if (size < OBD2_MIN_RESPONSE_SIZE) {
		return false;
	}

	if (respObdCmd == OBD2_SERVICE_MODE_3_RESPONSE) {
		char strDtcRecord = 0;
		uint8_t dtcRecord = 0;
		char strTokenDtcData[4] = { 0 };

		for (uint8_t u8 = 0; u8 < (size - FIRST_DTC_DATA_INDEX) / DTC_STR_DATA_SIZE; u8++) {
			memcpy(&strDtcRecord, &tokenArr[0][FIRST_DTC_DATA_INDEX + (u8 * DTC_STR_DATA_SIZE)],
			DTC_RECORD_SIZE);
			dtcRecord = (uint8_t) strtoul(&strDtcRecord, NULL, HEX_BASE_VALUE);
			memcpy(strTokenDtcData,
					&tokenArr[0][FIRST_DTC_STR_DATA_INDEX + (u8 * DTC_STR_DATA_SIZE)],
					DTC_STR_DATA_VALUE_SIZE);
			strTokenDtcData[DTC_STR_DATA_VALUE_SIZE] = '\0';
			sprintf(obd2VehicleData.vehicleDtcData[obd2VehicleData.vehicleDtcArrIndex++], "%s%s",
					strDTCs[dtcRecord], strTokenDtcData);
		}

		if (obd2VehicleData.vehicleDtcArrIndex) {
			obd2VehicleData.vehicleDtcArrIndex--;
		}
	}
	else {

		memcpy(strReplyPids, &tokenArr[0][OBD_RESP_PID_IDx], OBD2_REPLY_PIDs_STR_SIZE);
		obd2Pid = (uint8_t) strtoul(strReplyPids, NULL, HEX_BASE_VALUE);

		obd2Data = strtoul(&tokenArr[0][OBD_RESP_DATA_IDx], NULL, HEX_BASE_VALUE);

		switch (obd2Pid) {
		case PIDs_CALCULATE_ENGINE_LOAD:  // A * 100/255
			obd2VehicleData.calcEngineLoadValue = obd2Data * 100 / 255;
		break;
		case PIDs_ENGINE_COOLANT_TEMP:	 // A - 40
			obd2VehicleData.engineCoolantTemp = obd2Data - 40;
		break;
		case PIDs_FUEL_PRESSURE:	//  A * 3
			obd2VehicleData.fuelPressure = obd2Data * 3;
		break;
		case PIDs_ENGINE_SPEED:  // ((A * 256) + B) / 4
			obd2VehicleData.engineRPM = obd2Data / 4;
		break;
		case PIDs_VEHICLE_SPEED:  // A
			obd2VehicleData.vehicleSpeed = obd2Data;
		break;
		case PIDs_FUEL_LEVEL:  // A * 100 /255
			obd2VehicleData.fuelLevelInput = obd2Data * 100 / 255;
		break;
		case PIDs_ENGINE_FUEL_RATE:  // ((A*256) + B) * 0.05
			obd2VehicleData.engineFuelRate = obd2Data * 0.05;
		break;
		case PIDs_ENGINE_OIL_TEMP:  // A - 40
			obd2VehicleData.engineOilTemp = obd2Data - 40;
		break;
		case PIDs_ODOMETER_VALUE:  // (A*2^24 + B*2^16 + C*2^8 + D) / 10
			obd2VehicleData.vehicleOdometer = obd2Data / 10;
		break;
		default:
			return false;
		break;
		}
	}
	return true;
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
		if (obd2DataIDsIndex >= OBD2_PIDs_SIZE) {
							obd2DataIDsIndex = 0;}
		sprintf(&strPidsNumber[0], "%s\r", obd2DataIDs[obd2DataIDsIndex++]);
		if (!sendOBDUartData((uint8_t*) strPidsNumber, strlen(strPidsNumber))) {
			timeout = getOBDSystick();
			state = OBD2_GET_PIDs_DATA_WAIT;
		}
		break;
	}
	case OBD2_GET_PIDs_DATA_WAIT: {
		if (getOBDRecvCompleted()) {
			if (findOBDATCommandResp("UNABLE TO CONNECT")) {
				// OBD2 BAGLI DEGIL
				state = OBD2_GET_PIDs_DATA;
				obd2VehicleData.obd2SocketConnected = 0;
				break;
			}
			else if (findOBDATCommandResp("SEARCHING")) {
				state = OBD2_GET_PIDs_DATA;
			}
			else if (findOBDATCommandResp("?")) {
				state = OBD2_GET_PIDs_DATA;
			}
			else if (findOBDATCommandResp("NO DATA")) {
				if (obd2DataIDsIndex < OBD2_PIDs_SIZE) {
					state = OBD2_GET_PIDs_DATA;
					noDataCount++;
				}
				else if (noDataCount >= OBD2_PIDs_SIZE) {
					state = EXIT;
					obd2DataIDsIndex = 0;
					m_changeProtocolFlag = 1;
				}
			}
			else {
				noDataCount = 0;
				if(findOBDATCommandResp("ON")){
					memcpy(obd2VehicleData.vehicleIgnStr, "ON", 2);
				}
				else if(findOBDATCommandResp("OFF")){
					memcpy(obd2VehicleData.vehicleIgnStr, "OFF", 3);
				}
				convertObd2Data();
				state = OBD2_GET_PIDs_DATA;
				if (obd2DataIDsIndex >= OBD2_PIDs_SIZE) {
					obd2DataIDsIndex = 0;
					setObd2PeroidicDataCompletedState(OBD2_PERIODIC_DATA_COMPLETED);
				}

			}
			obd2VehicleData.obd2SocketConnected = 1;
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

	if (getOBDSystick() - timeout >= OBD_HAL_TIMEOUT_UNIT1MS(10000)) {
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
		if (getObd2GetPeriodicDataState() == OBD2_PERIODIC_DATA_TIMEOUT
				&& m_changeProtocolFlag == 1) {
			if (m_protocolTypeListIndex >= OBD2_PROTOCOL_TYPE_SIZE) {
				m_protocolTypeListIndex = 0;
			}
			if (changeOBD2Protocol(m_protocolTypeList[m_protocolTypeListIndex++]) == 0) {
				setObd2GetPeriodicDataState(OBD2_PERIODIC_DATA_START);
				m_changeProtocolFlag = 0;
			}
		}
		else {
			obd2GetPeriodicMsg();
		}
	}
}

void OBD_Virtual_Systick_Handler(void) {
	m_obdSystickCnt++;
//	if (getOBDSystick() - m_obd2DataTimeout >= OBD_HAL_TIMEOUT_UNIT1MS(300)) {
//		setOBDRecvCompleted(1);
//		stopObd2DataTimeout();
//	}
}

void OBD_Virtual_Rx_Completed_Callback(unsigned char rxData) {
	m_obdUartBuf[m_obdUartBuffCnt++] = rxData;
	//startObd2DataTimeout();
	if (rxData == '>') {
		setOBDRecvCompleted(1);
		//stopObd2DataTimeout();
	}

	if (m_obdUartBuffCnt >= OBD2_UART_RAW_DATA_SIZE) {
		m_obdUartBuffCnt = 0;
	}
}

