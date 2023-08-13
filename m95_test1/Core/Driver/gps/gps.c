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
#define GPS_UART_BUFF_COUNT		10

typedef struct {
	uint8_t uartBuff[255];
	uint8_t uartBuffCnt;
	uint8_t recvCompleted;
} gpsUartBuff_t;


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
	uint64_t recvData;
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
	uint64_t recvData;
} gpsNmeaRMCType_t;

gpsNmeaGGAType_t ggaMsg;
gpsNmeaRMCType_t rmcMsg;
char *ggatoken;
char *rmctoken;

UART_HandleTypeDef *m_gpsUart;
HAL_StatusTypeDef m_gpsRecvITError;

gpsUartBuff_t m_gpsUartBuff[GPS_UART_BUFF_COUNT];
uint8_t m_gpsUartBuffIndis;
uint8_t m_gpsRxData;

uint8_t searchNMEABuff[255];
uint8_t searchNMEABuffCnt;

uint32_t m_gpsSystick;

gpsNmeaSearchState_e m_gpsNmeaMsgSearchState = GPS_NMEA_MSG_SEARCH_FINISH;

void setGPSNmeaMsgSearchState(gpsNmeaSearchState_e state) {
	m_gpsNmeaMsgSearchState = state;
}

gpsNmeaSearchState_e getGPSNmeaMsgSearchState(void) {
	return m_gpsNmeaMsgSearchState;
}

void copyGPSUartBuffForNMEA(uint8_t indis) {
	if (getGPSNmeaMsgSearchState() == GPS_NMEA_MSG_SEARCH_FINISH) {
		searchNMEABuffCnt = m_gpsUartBuff[indis].uartBuffCnt;
		memcpy(searchNMEABuff, m_gpsUartBuff[indis].uartBuff, m_gpsUartBuff[indis].uartBuffCnt);
	}
}

uint32_t getGPSSystick(void) {
	return m_gpsSystick;
}

/**
 * @brief GPS init move UART_HandleTypeDef
 * @retval
 */
void gpsInit(void *uart) {
	m_gpsUart = (UART_HandleTypeDef*) uart;
	GPS_Virtual_Rx_IT();
}

void GPS_Virtual_Systick(void) {
	m_gpsSystick++;
}

void GPS_Virtual_Rx_IT(void) {
	m_gpsRecvITError = HAL_UART_Receive_IT(m_gpsUart, &m_gpsRxData, 1);
}

void GPS_Virtual_UART_RxCpltCallback(void *uart) {
	static uint8_t fcr = 0;
	static uint8_t prevcnt = 0;

	if (m_gpsUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		GPS_Virtual_Rx_IT();
		m_gpsUartBuff[m_gpsUartBuffIndis].uartBuff[m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt++] =
				m_gpsRxData;

		if (m_gpsRxData == '\r') {
			fcr = 1;
			prevcnt = m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt;
		}

		if (fcr && m_gpsRxData == '\n'
				&& (m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt - prevcnt) == 1) {
			m_gpsUartBuff[m_gpsUartBuffIndis].recvCompleted = 1;
			m_gpsUartBuffIndis++;
			fcr = 0;
		}

		if (m_gpsUartBuffIndis == GPS_UART_BUFF_COUNT) {
			m_gpsUartBuffIndis = 0;
		}
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
	for (uint8_t u8 = 1; u8 < (len - 5); u8++) {
		cs = cs ^ nmeaMsg[u8];
	}

	sprintf(strCrc, "%X", cs);

	if (strCrc[0] != nmeaMsg[len - 4] || strCrc[1] != nmeaMsg[len - 3]) {
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

	rmc->recvData++;
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

	gga->recvData++;

}

/**
 * @brief NMEA msg parser
 * @retval none
 */
void parseNmeaGGAandRMCMsg(void) {

//	static uint8_t queIndis = 0;
	for (uint8_t queIndis = 0; queIndis < GPS_UART_BUFF_COUNT; queIndis++) {
		if (m_gpsUartBuff[queIndis].recvCompleted) {
			copyGPSUartBuffForNMEA(queIndis);

			setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_IDLE);

			if (checkNMEAMsgValid(searchNMEABuff, searchNMEABuffCnt)) {
				// set flags
				setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_FINISH);
				m_gpsUartBuff[queIndis].recvCompleted = 0;
				m_gpsUartBuff[queIndis].uartBuffCnt = 0;
				return;
			}

			setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCHING);

			if (strstr((char*) searchNMEABuff, NMEA_GGA_MSG_HEADER) != NULL) {
				ggatoken = (char*) searchNMEABuff;
				convertGGAMsg(ggatoken, &ggaMsg);
			}
			else if (strstr((char*) searchNMEABuff, NMEA_RMC_MSG_HEADER) != NULL) {
				rmctoken = (char*) searchNMEABuff;
				convertRMCMsg(rmctoken, &rmcMsg);
			}

			setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_FINISH);

			m_gpsUartBuff[queIndis].recvCompleted = 0;
			m_gpsUartBuff[queIndis].uartBuffCnt = 0;
		}
	}
//	if (queIndis == GPS_UART_BUFF_COUNT) {
//		queIndis = 0;
//	}
}

void gpsControl(void) {
	if (m_gpsRecvITError != HAL_OK) {
		GPS_Virtual_Rx_IT();
	}

	//found msg
	parseNmeaGGAandRMCMsg();

}
