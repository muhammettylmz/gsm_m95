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

typedef enum{
	DISCONNECTED,
	CONNECTED
}connState_e;

typedef enum {
	BT_RECV_COMPLETED_IDLE, BT_RECV_COMPLETED
} btRecvComp_e;

connState_e m_btSerialConnState = DISCONNECTED;
connState_e m_btConnState = DISCONNECTED;
connState_e m_prevBtSerialConnState = DISCONNECTED;


UART_HandleTypeDef *m_btUart;
HAL_StatusTypeDef m_btRxIterror;

uint32_t m_btSystick;
uint32_t m_prevBtRxTimeoutCnt;
btRecvComp_e m_btRecvCompleted = BT_RECV_COMPLETED_IDLE;

uint8_t btRxRawBuff[128];
uint8_t btRxData;
uint8_t btRxRawBuffCnt;


uint8_t getBtSerialConnState(void){
	return (uint8_t)m_btSerialConnState;
}

uint32_t getBTSystick(void) {
	return m_btSystick;
}

btRecvComp_e getBTRecvCompleted(void) {
	return m_btRecvCompleted;
}

void setRecvCompleted(btRecvComp_e state) {
	m_btRecvCompleted = state;
}



void stopBTRxTimeout(void){
	m_prevBtRxTimeoutCnt = 0;
}

void startBTRxTimeout(void) {
	m_prevBtRxTimeoutCnt = getBTSystick();
	setRecvCompleted(BT_RECV_COMPLETED_IDLE);
}

void checkBTRxTimeoutCompleted(void) {
	if ((getBTSystick() - m_prevBtRxTimeoutCnt) >= HAL_TIMEOUT_UNIT1MS(5)) {
		setRecvCompleted(BT_RECV_COMPLETED);
		stopBTRxTimeout();
	}
}

void clearBtRxRawBuffCnt(void){
	btRxRawBuffCnt = 0;
}

void BT_Virtual_Systick(void) {
	m_btSystick++;
	if(m_prevBtRxTimeoutCnt != 0){
		checkBTRxTimeoutCompleted();
	}
}

void BT_Virtual_Rx_IT(void) {
	m_btRxIterror = HAL_UART_Receive_IT(m_btUart, &btRxData, 1);
}

void BT_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_btUart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		btRxRawBuff[btRxRawBuffCnt++] = btRxData;
		BT_Virtual_Rx_IT();
		startBTRxTimeout();
	}
}

void btInitConfig(void){
	//TODO: HM-10 Bluetooth konfigürasyon AT ayarları yapılacak.(GSM gibi yap)
	enum{
		BT_AT_SEND,
		BT_AT_SEND_WAIT,
		BT_ECHO_MODE,
		BT_ECHO_MODE_WAIT,
		BT_ROLE_MODE,
		BT_ROLE_MODE_WAIT,
	}btConfigState_e;

//	//use serial comm timeout for bt conn state disconnected.
//	goto labelBtConnState;
//
//	labelBtConnState:
	m_btSerialConnState = DISCONNECTED;
}

uint8_t sendBtUartData(uint8_t *data, uint16_t len) {
	clearBtRxRawBuffCnt();
	if( (HAL_UART_Transmit(m_btUart, data, len, 15)) != HAL_OK){
		return 1;
	}
	return 0;
}

void btInit(void *uart) {
	m_btUart = (UART_HandleTypeDef*) uart;
	BT_Virtual_Rx_IT();
	btInitConfig();
}

uint8_t getBtConnState(void){
	return (uint8_t)m_btConnState;
}

void checkBtConnState(void){
	if(getBtConnState() == CONNECTED){
		return;
	}

	if(getBtSerialConnState() != CONNECTED){
		btInitConfig();
		return;
	}

//	if(m_prevBtSerialConnState != m_btSerialConnState){
//		btInitConfig();
//		return;
//	}
	//TODO: bluetooth cihazı ile bağlantı işlemini burada yap.
}

void btSearchBTCommandResponse(void){
	if(getBTRecvCompleted() == BT_RECV_COMPLETED_IDLE && getBtConnState() != CONNECTED){
		return;
	}
}

void btControl(void) {
	if (m_btRxIterror != HAL_OK) {
		BT_Virtual_Rx_IT();
	}

	checkBtConnState();

	btSearchBTCommandResponse();
}

