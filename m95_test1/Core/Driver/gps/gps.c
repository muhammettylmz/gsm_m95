/*
 * gps.c
 *
 *  Created on: Aug 12, 2023
 *      Author: muham
 */
#include "gps.h"
#include "main.h"

typedef enum {
	GPS_MSG_NOT_FOUND, GPS_MSG_FOUND
} gpsMsgFound_e;

typedef enum {
	GPS_PARSER_START_CHR, GPS_PARSER_VALUE, GPS_PARSER_FOUND_CR,
} gpsParserState_e;

typedef enum{
	GPS_UART_TIMEOUT_IDLE,
	GPS_UART_TIMEOUT
}gpsUartTimeoutState_e;

UART_HandleTypeDef *m_gpsUart;
HAL_StatusTypeDef m_gpsRecvITError;
uint8_t m_gpsRawBuf[128];
uint8_t m_gpsRawCnt;
uint8_t m_gpsRxData;

gpsMsgFound_e m_gpsMsgFoundState;
gpsParserState_e m_gpsParserState = GPS_PARSER_START_CHR;

uint32_t m_gpsSystick;
uint32_t m_gpsUartTimeoutCnt = 0;
gpsUartTimeoutState_e m_gpsUartTimeout;


uint32_t getGPSSystick(void){
	return m_gpsSystick;
}

void startGPsUartTimeoutCnt(void){
	m_gpsUartTimeoutCnt = getGPSSystick();
}

uint32_t getGPSUartTimeoutCnt(void){
	return m_gpsUartTimeoutCnt;
}

void setGPSParserState(gpsParserState_e state){
	m_gpsParserState = state;
}

gpsParserState_e getGPSParserState(void){
	return m_gpsParserState;
}

void setGPSMsgFoundState(gpsMsgFound_e state) {
	m_gpsMsgFoundState = state;
}

gpsMsgFound_e getGPSMsgFoundState(void) {
	return m_gpsMsgFoundState;
}

void checkGPSUartTimeoutCnt(void){
	if(getGPSSystick() - getGPSUartTimeoutCnt() >= 5){
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
			m_gpsRawBuf[m_gpsRawCnt++] = chr;
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
		gpsUartParser(m_gpsRxData);
		GPS_Virtual_Rx_IT();
		startGPsUartTimeoutCnt();
	}
}

void gpsControl(void) {
	if (m_gpsRecvITError != HAL_OK) {
		GPS_Virtual_Rx_IT();
	}

	//found msg
//	if(getGPSMsgFound() == GPS_MSG_FOUND){
//		getNmeaGGAandRMC();
//	}
}
