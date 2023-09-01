/*
 * bluetooth.c
 *
 *  Created on: Aug 19, 2023
 *      Author: muham
 */

#include "bluetooth.h"
#include "main.h"
#include "uart_debug.h"
#include "m95.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HAL_TIMEOUT_UNIT1MS(x)	(x)

#define BT_UART_BUFF_ARR_SIZE	128
#define BT_UART_BUFF_SIZE		50

typedef enum {
	BT_RECV_COMPLETED_IDLE, BT_RECV_COMPLETED
} btRecvComp_e;

typedef struct {
	uint8_t buff[BT_UART_BUFF_ARR_SIZE];
	uint8_t buffCnt;
	uint8_t recvCompleted;
} btUartBuff_t;

UART_HandleTypeDef *m_btUart;
HAL_StatusTypeDef m_btRxIterror;

uint32_t m_btSystick;
uint32_t m_prevBtRxTimeoutCnt;
btRecvComp_e m_btRecvCompleted = BT_RECV_COMPLETED_IDLE;

btUartBuff_t m_btUartRaw[BT_UART_BUFF_SIZE];
uint16_t m_btUartBuffIndis;
uint8_t btRxData;
char *m_btToken;

uint32_t getBTSystick(void) {
	return m_btSystick;
}

btRecvComp_e getBTRecvCompleted(void) {
	return m_btRecvCompleted;
}

void setRecvCompleted(btRecvComp_e state) {
	m_btRecvCompleted = state;
}

void checkBTRxTimeoutCompleted(void) {
	if ((getBTSystick() - m_prevBtRxTimeoutCnt) >= HAL_TIMEOUT_UNIT1MS(5)) {
		setRecvCompleted(BT_RECV_COMPLETED);
	}
}

void startBTRxTimeout(void) {
	m_prevBtRxTimeoutCnt = getBTSystick();
	setRecvCompleted(BT_RECV_COMPLETED_IDLE);
}

void BT_Virtual_Systick(void) {
	m_btSystick++;
}

void BT_Virtual_Rx_IT(void) {
	m_btRxIterror = HAL_UART_Receive_IT(m_btUart, &btRxData, 1);
}

void BT_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_btUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {

//		m_btUartRaw[m_btUartBuffIndis].buff[m_btUartRaw[m_btUartBuffIndis].buffCnt++] = btRxData;
//
//		if (btRxData == '\n'
//				&& (m_btUartRaw[m_btUartBuffIndis].buff[m_btUartRaw[m_btUartBuffIndis].buffCnt - 2]
//						== '\r')) {
//			m_btUartRaw[m_btUartBuffIndis].recvCompleted = 1;
//			m_btUartBuffIndis++;
//		}
//
//		if (m_btUartRaw[m_btUartBuffIndis].buffCnt > BT_UART_BUFF_ARR_SIZE) {
//			m_btUartRaw[m_btUartBuffIndis].buffCnt = 0;
//		}
//
//		if (m_btUartBuffIndis >= BT_UART_BUFF_SIZE) {
//			m_btUartBuffIndis = 0;
//		}

		BT_Virtual_Rx_IT();
		startBTRxTimeout();
		obd2DebugUart(btRxData);
	}
}

void btInit(void *uart) {
	m_btUart = (UART_HandleTypeDef*) uart;
	BT_Virtual_Rx_IT();
}

void sendBtUartData(uint8_t* data, uint16_t len){
//	static uint8_t btTxdata = 0;
//	btTxdata = data;
	HAL_UART_Transmit(m_btUart, data, len, 5);
}

uint8_t checkValidMsg(uint8_t *data, uint16_t len) {
	(void) data;
	(void) len;
	return 1;
}

void parserOBD2(void) {
	for (uint8_t bufIndis = 0; bufIndis < BT_UART_BUFF_SIZE; bufIndis++) {
		if (m_btUartRaw[m_btUartBuffIndis].recvCompleted) {

			m_btToken = strtok((char*) m_btUartRaw[m_btUartBuffIndis].buff, "\r\n");

			if (!checkValidMsg((uint8_t*) m_btToken, strlen(m_btToken))) {
				m_btUartRaw[m_btUartBuffIndis].recvCompleted = 0;
				m_btUartRaw[m_btUartBuffIndis].buffCnt = 0;
			}
			else {
				m_btUartRaw[m_btUartBuffIndis].recvCompleted = 0;
				m_btUartRaw[m_btUartBuffIndis].buffCnt = 0;
			}
		}
	}
}

void btControl(void) {
	if (m_btRxIterror != HAL_OK) {
		BT_Virtual_Rx_IT();
	}

	parserOBD2();
}

