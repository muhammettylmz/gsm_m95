/*
 * gps.c
 *
 *  Created on: Aug 12, 2023
 *      Author: muham
 */
#include "gps.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define GPS_MPH_PER_KNOT 			1.15077945
#define GPS_MPS_PER_KNOT 			0.51444444
#define GPS_KMPH_PER_KNOT 			1.852
#define GPS_MILES_PER_METER 		0.00062137112
#define GPS_KM_PER_METER 			0.001
#define GPS_FEET_PER_METER 			3.2808399

#define NMEA_DELIMETER_STRCHR		','
#define NMEA_GGA_MSG_HEADER			(char*)"$GPGGA"
#define NMEA_RMC_MSG_HEADER			(char*)"$GPRMC"
#define NMEA_GSA_MSG_HEADER			(char*)"$GPGSA"

#define GPS_UART_BUFF_COUNT			15
#define GPS_UART_BUFF_SIZE			100

typedef struct {
	uint8_t uartBuff[GPS_UART_BUFF_SIZE];
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

typedef struct {
	char sMode;  // M: Manual - forced to operate in 2D or 3D mode
				 // A: Allowed to automatically switch 2D/3D mode
	uint8_t fixStatus;
	float pdop;
	float hdop;
	float vdop;
	uint64_t recvData;
} gpsNmeaGSAType_t;

gpsNmeaGGAType_t lastValidGgaMsg;
gpsNmeaGGAType_t ggaMsg;
gpsNmeaRMCType_t rmcMsg;
gpsNmeaRMCType_t lastValidRmcMsg;
gpsNmeaGSAType_t gsaMsg;
gpsNmeaGSAType_t lastValidGsaMsg;

UART_HandleTypeDef *m_gpsUart;
HAL_StatusTypeDef m_gpsRecvITError;

gpsUartBuff_t m_gpsUartBuff[GPS_UART_BUFF_COUNT];
uint8_t m_gpsUartBuffIndis;
uint8_t m_gpsRxData;

char* m_nmeaToken;

uint32_t m_gpsSystick;

gpsNmeaSearchState_e m_gpsNmeaMsgSearchState = GPS_NMEA_MSG_SEARCH_FINISH;

void setGPSNmeaMsgSearchState(gpsNmeaSearchState_e state) {
	m_gpsNmeaMsgSearchState = state;
}

gpsNmeaSearchState_e getGPSNmeaMsgSearchState(void) {
	return m_gpsNmeaMsgSearchState;
}

uint32_t getGPSSystick(void) {
	return m_gpsSystick;
}

void getGGALatLongValue(double *_lat, double *_long) {
	if (ggaMsg.numberOfSatellites > 2 && ggaMsg.quality > 0) {
		*_lat = ggaMsg.latitude;
		*_long = ggaMsg.longitude;
	}
	else {
		*_lat = 0;
		*_long = 0;
	}
}

void getRMCLatLongValue(double *_lat, double *_long) {
	if (rmcMsg.status == 'A') {
		*_lat = rmcMsg.latitude;
		*_long = rmcMsg.longitude;
	}
	else {
		*_lat = 0;
		*_long = 0;
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

void GPS_Virtual_Systick(void) {
	m_gpsSystick++;
}

void GPS_Virtual_Rx_IT(void) {
	m_gpsRecvITError = HAL_UART_Receive_IT(m_gpsUart, &m_gpsRxData, 1);
}

void GPS_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_gpsUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {

		//stored nmea msg into uart buff
		m_gpsUartBuff[m_gpsUartBuffIndis].uartBuff[m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt++] =
				m_gpsRxData;

		// find nmea msg linefeed
		if (m_gpsRxData == '\n'
				&& (m_gpsUartBuff[m_gpsUartBuffIndis].uartBuff[m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt
						- 2]) == '\r') {
			m_gpsUartBuff[m_gpsUartBuffIndis].recvCompleted = 1;
			m_gpsUartBuffIndis++;
		}

		//clear buffer size for there is anamoly
		if ((m_gpsUartBuffIndis < GPS_UART_BUFF_COUNT)
				&& m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt >= GPS_UART_BUFF_SIZE) {
			m_gpsUartBuff[m_gpsUartBuffIndis].uartBuffCnt = 0;
		}

		//	index buffer size clearing for there is circular buffer
		if (m_gpsUartBuffIndis == GPS_UART_BUFF_COUNT) {
			m_gpsUartBuffIndis = 0;
		}

		// RXD IT enable
		GPS_Virtual_Rx_IT();
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
	for (uint8_t u8 = 1; u8 < (len - 3); u8++) {  // without \r\n
		cs = cs ^ nmeaMsg[u8];
	}

	sprintf(strCrc, "%.2X", cs);

	if (strCrc[0] != nmeaMsg[len - 2] || strCrc[1] != nmeaMsg[len - 1]) {  // without \r\n
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

	if(rmc->status == 'A'){
		lastValidRmcMsg = *rmc;
		//memcpy(&lastValidRmcMsg, rmc , sizeof(lastValidRmcMsg));
	}
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

	if(gga->quality > 0){
		lastValidGgaMsg = *gga;
//		memcpy(&lastValidGgaMsg ,gga , sizeof(lastValidGgaMsg));
	}

}

/**
 * @brief find GSA field value in NMEA msg
 * @retval none
 */
void convertGSAMsg(char *msg, gpsNmeaGSAType_t *gsa) {

	char *tempgsa;
	// sMode
	tempgsa = strchr(msg, NMEA_DELIMETER_STRCHR);
	gsa->sMode = tempgsa[1];

	//fix status
	tempgsa = strchr(tempgsa + 1, NMEA_DELIMETER_STRCHR);
	gsa->fixStatus = atoi(tempgsa + 1);

	for (uint8_t u8 = 0; u8 < 12; u8++) {
		tempgsa = strchr(tempgsa + 1, NMEA_DELIMETER_STRCHR);
	}

	//pdop
	tempgsa = strchr(tempgsa + 1, NMEA_DELIMETER_STRCHR);
	gsa->pdop = atoff(tempgsa + 1);
	//hdop
	tempgsa = strchr(tempgsa + 1, NMEA_DELIMETER_STRCHR);
	gsa->hdop = atoff(tempgsa + 1);
	//vdop
	tempgsa = strchr(tempgsa + 1, NMEA_DELIMETER_STRCHR);
	gsa->vdop = atoff(tempgsa + 1);

	gsa->recvData++;
	if(gsa->fixStatus > 1){
		lastValidGsaMsg = *gsa;
//		memcpy(&lastValidGsaMsg ,gsa , sizeof(lastValidGsaMsg));
	}
}

/**
 * @brief NMEA msg parser
 * @retval none
 */
void parseNmeaMsg(void) {
	for (uint8_t queIndis = 0; queIndis < GPS_UART_BUFF_COUNT; queIndis++) {
		if (m_gpsUartBuff[queIndis].recvCompleted) {

			setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_IDLE);

			m_nmeaToken = strtok((char*) m_gpsUartBuff[queIndis].uartBuff, "\r\n");

			if (checkNMEAMsgValid((uint8_t*) m_nmeaToken, strlen(m_nmeaToken))) {
				// set flags
				m_gpsUartBuff[queIndis].recvCompleted = 0;
				m_gpsUartBuff[queIndis].uartBuffCnt = 0;
			}
			else {
				setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCHING);

				if (strncmp(m_nmeaToken, NMEA_GSA_MSG_HEADER, (size_t)sizeof(NMEA_GSA_MSG_HEADER)) == 0) {
					convertGSAMsg(m_nmeaToken, &gsaMsg);
				}
				else if (strncmp(m_nmeaToken, NMEA_GGA_MSG_HEADER, (size_t)sizeof(NMEA_GGA_MSG_HEADER)) == 0) {
					convertGGAMsg(m_nmeaToken, &ggaMsg);
				}
				else if (strncmp(m_nmeaToken, NMEA_RMC_MSG_HEADER, (size_t)sizeof(NMEA_RMC_MSG_HEADER)) == 0) {
					convertRMCMsg(m_nmeaToken, &rmcMsg);
				}

				setGPSNmeaMsgSearchState(GPS_NMEA_MSG_SEARCH_FINISH);

				m_gpsUartBuff[queIndis].recvCompleted = 0;
				m_gpsUartBuff[queIndis].uartBuffCnt = 0;
			}
		}
	}
}

void gpsControl(void) {

	// RX IT re-trigger
	if (m_gpsRecvITError != HAL_OK) {
		GPS_Virtual_Rx_IT();
	}

	//find nmea msg into gps uart circular buffer
	parseNmeaMsg();
}
