/*
 * gps.c
 *
 *  Created on: Aug 12, 2023
 *      Author: muham
 */
#include "gps.h"
#include "main.h"
#include "uart_debug.h"
#include <string.h>
#include <stdio.h>

#define NMEA_DELIMETER			(const char*)","
#define NMEA_GGA_MSG_ID			(char*)"GPGGA"
#define NMEA_RMC_MSG_ID			(char*)"GPRMC"
#define NMEA_RMC_FIELD_SIZE		15
#define NMEA_GGA_FIELD_SIZE 	17

typedef enum {
	GPS_MSG_NOT_FOUND, GPS_MSG_FOUND
} gpsMsgFound_e;

typedef enum {
	GPS_PARSER_START_CHR, GPS_PARSER_VALUE, GPS_PARSER_FOUND_CR,
} gpsParserState_e;

typedef enum {
	GPS_UART_TIMEOUT_IDLE, GPS_UART_TIMEOUT
} gpsUartTimeoutState_e;

typedef enum {
	GPS_NMEA_MSG_SEARCH_IDLE, GPS_NMEA_MSG_SEARCHING, GPS_NMEA_MSG_SEARCH_FINISH
} gpsNmeaSearchState_e;

UART_HandleTypeDef *m_gpsUart;
HAL_StatusTypeDef m_gpsRecvITError;
uint8_t m_gpsRawBuf[255];
uint8_t m_gpsRawCnt;
uint8_t m_gpsRxData;

uint8_t searchNMEABuff[255];
uint8_t searchNMEABuffCnt;

gpsMsgFound_e m_gpsMsgFoundState;
gpsParserState_e m_gpsParserState = GPS_PARSER_START_CHR;

uint32_t m_gpsSystick;
uint32_t m_gpsUartTimeoutCnt = 0;
gpsUartTimeoutState_e m_gpsUartTimeout;

gpsNmeaSearchState_e m_gpsNmeaMsgSearchState = GPS_NMEA_MSG_SEARCH_FINISH;

char *ggatoken;
char *rmctoken;
char *fieldTokenGGA[NMEA_GGA_FIELD_SIZE];
char *fieldTokenRMC[NMEA_RMC_FIELD_SIZE];

void parseNmeaGGAandRMCMsg(void);

void setGPSNmeaMsgSearchState(gpsNmeaSearchState_e state) {
	m_gpsNmeaMsgSearchState = state;
}

gpsNmeaSearchState_e getGPSNmeaMsgSearchState(void) {
	return m_gpsNmeaMsgSearchState;
}

void copyGPSUartBuffForNMEA(void) {
	if (getGPSNmeaMsgSearchState() == GPS_NMEA_MSG_SEARCH_FINISH) {
		searchNMEABuffCnt = m_gpsRawCnt;
		memcpy(searchNMEABuff, m_gpsRawBuf, m_gpsRawCnt);
	}
}

uint32_t getGPSSystick(void) {
	return m_gpsSystick;
}

void startGPsUartTimeoutCnt(void) {
	m_gpsUartTimeoutCnt = getGPSSystick();
}

uint32_t getGPSUartTimeoutCnt(void) {
	return m_gpsUartTimeoutCnt;
}

void setGPSParserState(gpsParserState_e state) {
	m_gpsParserState = state;
}

gpsParserState_e getGPSParserState(void) {
	return m_gpsParserState;
}

void setGPSMsgFoundState(gpsMsgFound_e state) {
	m_gpsMsgFoundState = state;
}

gpsMsgFound_e getGPSMsgFoundState(void) {
	return m_gpsMsgFoundState;
}

void checkGPSUartTimeoutCnt(void) {
	if (getGPSSystick() - getGPSUartTimeoutCnt() >= 5) {
		m_gpsUartTimeout = GPS_UART_TIMEOUT;
	}
}

/**
 * @brief GPS init move UART_HandleTypeDef
 * @retval
 */
void gpsInit(void *uart) {
	m_gpsUart = (UART_HandleTypeDef*) uart;
	GPS_Virtual_Rx_IT();
	memcpy(searchNMEABuff,
			(uint8_t*) "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B",
			73);
	searchNMEABuffCnt = 73;

	parseNmeaGGAandRMCMsg();
}

/**
 * @brief NEO-6 gps parser
 * @retval
 */
void gpsUartParser(uint8_t chr) {
	switch (m_gpsParserState) {
	case GPS_PARSER_START_CHR: {
		if (chr == '$') {
			m_gpsRawCnt = 0;
			m_gpsParserState = GPS_PARSER_VALUE;
			setGPSMsgFoundState(GPS_MSG_NOT_FOUND);
		}
		break;
	}
	case GPS_PARSER_VALUE: {
		if (chr == '\r') {
			m_gpsParserState = GPS_PARSER_FOUND_CR;
		}
		else {
			m_gpsRawBuf[m_gpsRawCnt++] = chr;  // not include '\r' and '\n'
		}
		break;
	}
	case GPS_PARSER_FOUND_CR: {
		if (chr == '\n') {
			setGPSMsgFoundState(GPS_MSG_FOUND);
		}
		else {
			m_gpsParserState = GPS_PARSER_START_CHR;
		}
		break;
	}
	default:
		// not found
		m_gpsParserState = GPS_PARSER_START_CHR;
	break;
	}

}

void GPS_Virtual_Systick(void) {
	m_gpsSystick++;
	checkGPSUartTimeoutCnt();
}

void GPS_Virtual_Rx_IT(void) {
	m_gpsRecvITError = HAL_UART_Receive_IT(m_gpsUart, &m_gpsRxData, 1);
}

void GPS_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_gpsUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		GPS_Virtual_Rx_IT();
		gpsUartParser(m_gpsRxData);
		startGPsUartTimeoutCnt();
	}
}

typedef struct {
	uint32_t time;  //hhmmss
	float latitude;
	float longitude;
	uint32_t altitude;
	uint8_t quality;
	uint8_t numberOfSatellites;
	float hdop;
} gpsNmeaGGAType_t;

gpsNmeaGGAType_t ggaMsg;

/**
 * @brief Calculate NMEA msg checksum
 * @retval 0 is succes, others 1
 */
uint8_t checkNMEAMsgValid(uint8_t *nmeaMsg, uint8_t len) {
	char strCrc[3] = { 0 };
	uint8_t cs = 0;

	//without $(0.index) and *(3. to last) not include \r\n
	for (uint8_t u8 = 1; u8 < (len - 3); u8++) {
		cs = cs ^ nmeaMsg[u8];
	}

	sprintf(strCrc, "%X", cs);

	if (strCrc[0] != nmeaMsg[len - 2] || strCrc[1] != nmeaMsg[len - 1]) {
		return 1;
	}
	return 0;
}

void parseNmeaGGAandRMCMsg(void) {
	if (checkNMEAMsgValid(searchNMEABuff, searchNMEABuffCnt)) {
		// set flags
		return;
	}

	ggatoken = (char*) searchNMEABuff;
	rmctoken = (char*) searchNMEABuff;

	if (strstr((char*) searchNMEABuff, NMEA_GGA_MSG_ID) != NULL) {

		//fieldTokenGGA[0] = strtok(ggatoken, NMEA_DELIMETER);
		for (uint8_t u8 = 0; u8 < NMEA_GGA_FIELD_SIZE; u8++) {
			fieldTokenGGA[u8] = strtok_r(ggatoken, NMEA_DELIMETER, &ggatoken);
			customDebugMsg("fieldTokenGGA[%d] : %s\r\n", u8, fieldTokenGGA[u8]);
		}
	}
	else if (strstr((char*) searchNMEABuff, NMEA_RMC_MSG_ID) != NULL) {
		for (uint8_t u8 = 0; u8 < NMEA_RMC_FIELD_SIZE; u8++) {
			fieldTokenRMC[u8] = strtok_r(rmctoken, NMEA_DELIMETER, &rmctoken);
			customDebugMsg("fieldTokenRMC[%d] : %s\r\n", u8, fieldTokenRMC[u8]);
		}
	}
}

void gpsControl(void) {
	if (m_gpsRecvITError != HAL_OK) {
		GPS_Virtual_Rx_IT();
	}

	//found msg
	if (getGPSMsgFoundState() == GPS_MSG_FOUND) {
		copyGPSUartBuffForNMEA();
		parseNmeaGGAandRMCMsg();
	}
}
