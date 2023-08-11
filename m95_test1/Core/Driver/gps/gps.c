/*
 * gps.c
 *
 *  Created on: Aug 12, 2023
 *      Author: muham
 */
#include "gps.h"
#include "main.h"

uint8_t m_gpsRawBuf[128];
uint8_t m_gpsRawCnt;
uint8_t m_gpsRxData;
UART_HandleTypeDef *m_gpsUart;
HAL_StatusTypeDef m_gpsRecvITError;
uint32_t m_gpsSystick;

/**
 * @brief GPS init move UART_HandleTypeDef
 * @retval
 */
void gpsInit(void* uart){
	m_gpsUart = (UART_HandleTypeDef*)uart;
	GPS_Virtual_Rx_IT();
}

/**
 * @brief NEO-6 gps parser
 * @retval
 */
void gpsUartParser(uint8_t chr){
	static uint8_t state = 0;
	static  uint8_t foundCr = 0;
	switch(state){
	case 0:{
		if(chr == '$'){
			state++;
		}
		break;
	}
	case 1:{
		if(chr == '\r'){
			// found cr
			foundCr = 1;
		}

		if(foundCr && chr == '\n'){
			// msg found
			state = 0;
			foundCr = 0;
		}
		m_gpsRawBuf[m_gpsRawCnt++] = chr;
		break;
	}
	default:
		// not found
		state = 0;
		foundCr = 0;
		m_gpsRawCnt = 0;
		break;
	}

}

void GPS_Virtual_Systick(void){
	m_gpsSystick++;
}

void GPS_Virtual_Rx_IT(void){
	m_gpsRecvITError = HAL_UART_Receive_IT(m_gpsUart, &m_gpsRxData, 1);
}

void GPS_Virtual_UART_RxCpltCallback(void *uart){
	if(m_gpsUart->Instance == ((UART_HandleTypeDef*)uart)->Instance){
		gpsUartParser(m_gpsRxData);
		GPS_Virtual_Rx_IT();
	}
}

void gpsControl(void){
	if(m_gpsRecvITError != HAL_OK){
		GPS_Virtual_Rx_IT();
	}

	//found msg
//	if(getGPSMsgFound() == GPS_MSG_FOUND){
//		getNmeaGGAandRMC();
//	}
}
