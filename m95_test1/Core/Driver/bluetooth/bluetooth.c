/*
 * bluetooth.c
 *
 *  Created on: Aug 19, 2023
 *      Author: muham
 */

#include "bluetooth.h"
#include "main.h"
#include "uart_debug.h"
#include <stdlib.h>
#include <stdio.h>

typedef enum{
	BT_RECV_COMPLETED_IDLE,
	BT_RECV_COMPLETED
}btRecvComp_e;

uint32_t m_btSystick;
UART_HandleTypeDef *m_btUart;
HAL_StatusTypeDef m_btRxIterror;
uint8_t btRxData;
uint8_t btUartBuff[128];
uint8_t btUartCnt;
uint32_t m_prevBtRxTimeoutCnt;
btRecvComp_e m_btRecvCompleted = BT_RECV_COMPLETED_IDLE;

uint32_t getBTSystick(void){
	return m_btSystick;
}

btRecvComp_e getBTRecvCompleted(void){
	return m_btRecvCompleted;
}

void setRecvCompleted(btRecvComp_e state){
	m_btRecvCompleted = state;
}

void checkBTRxTimeoutCompleted(void){
	if((getBTSystick()- m_prevBtRxTimeoutCnt) >= 5){
		setRecvCompleted(BT_RECV_COMPLETED);
	}
}

void startBTRxTimeout(void){
	m_prevBtRxTimeoutCnt = getBTSystick();
	setRecvCompleted(BT_RECV_COMPLETED_IDLE);
}

void BT_Virtual_Systick(void){
	m_btSystick++;
}

void BT_Virtual_Rx_IT(void){
	m_btRxIterror = HAL_UART_Receive_IT(m_btUart, &btRxData, 1);
}

void BT_Virtual_UART_RxCpltCallback(void *uart){
	if(m_btUart->Instance == ((UART_HandleTypeDef*)uart)->Instance){
		BT_Virtual_Rx_IT();
		startBTRxTimeout();
	}
}

void btInit(void* uart){
	m_btUart = (UART_HandleTypeDef*)uart;
}

void btControl(void){
	if(m_btRxIterror != HAL_OK){
		BT_Virtual_Rx_IT();
	}
}


