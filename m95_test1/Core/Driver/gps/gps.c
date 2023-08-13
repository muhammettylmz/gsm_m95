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
#include <stdlib.h>
#include <math.h>

#define NMEA_DELIMETER_STRCHR	','
#define NMEA_GGA_MSG_HEADER		(char*)"GPGGA"
#define NMEA_RMC_MSG_HEADER		(char*)"GPRMC"
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

typedef struct {
	uint32_t time;  //hhmmss
	double latitude;
	char ns;
	double longitude;
	char ew;
	float altitude;
	uint8_t quality;
	uint8_t numberOfSatellites;
	float hdop;
} gpsNmeaGGAType_t;

typedef struct {
	uint32_t time;  //hhmmss
	char status;
	double latitude;
	char ns;
	double longitude;
	char ew;
	float speedKnots;
	float cog;
} gpsNmeaRMCType_t;

gpsNmeaGGAType_t ggaMsg;
gpsNmeaRMCType_t rmcMsg;
char *ggatoken;
char *rmctoken;

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

/**
 * @brief Calculate NMEA msg to lat long degree
 * @retval double degree
 */
double convertNMEAtoDegree(double nmeaVal, char indicator) {

	double value;
	value = (double) (fmod(nmeaVal, 100.0) / 60.0) + (double) ((uint8_t) (nmeaVal / 100));

	if (indicator == 'W' || indicator == 'S') {
		value *= -1;
	}
	return value;
}

/**
 * @brief find rmc field value in NMEA msg
 * @retval none
 */
void convertRMCMsg(char *msg, gpsNmeaRMCType_t *rmc) {
	char *temprmc;
	// time
	temprmc = strchr(msg, NMEA_DELIMETER_STRCHR);
	rmc->time = atol(temprmc + 1);
	//
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->status = temprmc[1];  // V is recv warnig or A is  data valid

	//latitude
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->latitude = atof(temprmc + 1);

	//latitude N/S Indicator
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->ns = temprmc[1] == ',' ? '?' : temprmc[1];

	rmc->latitude = convertNMEAtoDegree(rmc->latitude, rmc->ns);

	//longitude
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->longitude = atoff(temprmc + 1);

	//longitude E/W Indicator
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->ew = temprmc[1] == ',' ? '?' : temprmc[1];

	rmc->longitude = convertNMEAtoDegree(rmc->longitude, rmc->ew);

	// speed over ground
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->speedKnots = atoff(temprmc + 1);

	// Course over grond
	temprmc = strchr(temprmc + 1, NMEA_DELIMETER_STRCHR);
	rmc->cog = atoff(temprmc + 1);

}

/**
 * @brief find gga field value in NMEA msg
 * @retval none
 */
void convertGGAMsg(char *msg, gpsNmeaGGAType_t *gga) {
	char *tempgga;
	// time
	tempgga = strchr(msg, NMEA_DELIMETER_STRCHR);
	gga->time = atol(tempgga + 1);

	//latitude
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->latitude = atof(tempgga + 1);

	//latitude N/S Indicator
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->ns = tempgga[1] == ',' ? '?' : tempgga[1];

	gga->latitude = convertNMEAtoDegree(gga->latitude, gga->ns);

	//longitude
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->longitude = atoff(tempgga + 1);

	//longitude E/W Indicator
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->ew = tempgga[1] == ',' ? '?' : tempgga[1];

	gga->longitude = convertNMEAtoDegree(gga->longitude, gga->ew);

	//
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->quality = atoi(tempgga + 1);

	//
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->numberOfSatellites = atoi(tempgga + 1);

	//
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->hdop = atoff(tempgga + 1);

	//
	tempgga = strchr(tempgga + 1, NMEA_DELIMETER_STRCHR);
	gga->altitude = atoff(tempgga + 1);

}

/**
 * @brief NMEA msg parser
 * @retval none
 */
void parseNmeaGGAandRMCMsg(void) {
	setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_IDLE);
	if (checkNMEAMsgValid(searchNMEABuff, searchNMEABuffCnt)) {
		// set flags
		setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_FINISH);
		return;
	}

	setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCHING);

	ggatoken = (char*) searchNMEABuff;
	rmctoken = (char*) searchNMEABuff;

	if (strstr((char*) searchNMEABuff, NMEA_GGA_MSG_HEADER) != NULL) {
		convertGGAMsg(ggatoken, &ggaMsg);
	}
	else if (strstr((char*) searchNMEABuff, NMEA_RMC_MSG_HEADER) != NULL) {
		convertRMCMsg(rmctoken, &rmcMsg);
	}
	setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_FINISH);
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
