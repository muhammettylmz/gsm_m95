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
	BT_CONFIG_START, BT_CONFIG_FINISH, BT_CONFIG_TIMEOUT
} btConfigState_e;
typedef enum {
	BT_CONNECTION_START, BT_CONNECTION_FINISH, BT_CONNECTION_TIMEOUT
} btConnectionState_e;

typedef enum {
	DISCONNECTED, CONNECTED
} connState_e;

typedef enum {
	BT_RECV_COMPLETED_IDLE, BT_RECV_COMPLETED
} btRecvComp_e;

btConfigState_e m_btConfigState = BT_CONFIG_START;
btConnectionState_e m_btConnectionState = BT_CONNECTION_START;

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

uint8_t getBtConfigState(void) {
	return (uint8_t) m_btConfigState;
}

void setBtConfigState(btConfigState_e state) {
	m_btConfigState = state;
}

uint8_t getBtConnectionState(void) {
	return (uint8_t) m_btConnectionState;
}

void setBtConnectionState(btConnectionState_e state) {
	m_btConnectionState = state;
}

uint8_t getBtSerialConnState(void) {
	return (uint8_t) m_btSerialConnState;
}

uint32_t getBTSystick(void) {
	return m_btSystick;
}

uint8_t getBTRecvCompleted(void) {
	return (uint8_t) m_btRecvCompleted;
}

void setRecvCompleted(btRecvComp_e state) {
	m_btRecvCompleted = state;
}

void stopBTRxTimeout(void) {
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

void clearBtRxRawBuffCnt(void) {
	btRxRawBuffCnt = 0;
}

void BT_Virtual_Systick(void) {
	m_btSystick++;
	if (m_prevBtRxTimeoutCnt != 0) {
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

void clearBtStateFlags(void) {
	m_btConfigState = BT_CONFIG_START;
	m_btConnState = DISCONNECTED;
	m_btSerialConnState = DISCONNECTED;

}

uint8_t findBTATCommandResp(char *resp) {
	return strstr((char*) btRxRawBuff, resp) != NULL;
}

void btInitConfig(void) {
	//TODO: HM-10 Bluetooth konfigürasyon AT ayarları yapılacak.(GSM gibi yap)
	typedef enum {
		BT_AT_SEND,
		BT_AT_SEND_WAIT,
		BT_IMME_SEND,
		BT_IMME_WAIT,
		BT_ROLE_MODE,
		BT_ROLE_MODE_WAIT,
		BT_MODE,
		BT_MODE_WAIT,
		BT_SHOW_SEND,
		BT_SHOW_SEND_WAIT,
		BT_RESET_SEND,
		BT_RESET_SEND_WAIT,
		BT_START_SEND,
		BT_START_SEND_WAIT,
		EXIT
	} btConfigState_e;

	static btConfigState_e initState = BT_AT_SEND;
	static uint8_t retry = 0;
	static uint32_t timeout = 0;
	uint32_t waitResponseTimeout = 300;

	if (timeout == 0) {
		timeout = getBTSystick();
	}
	m_btConfigState = BT_CONFIG_START;

	switch (initState) {
	case BT_AT_SEND:
		if (!sendBtUartData((uint8_t*) "AT\n\r", sizeof((uint8_t*) "AT\n\r"))) {
			timeout = getBTSystick();
			initState = BT_AT_SEND_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_AT_SEND_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK")) {
				initState = BT_IMME_SEND;
			}
		}
	break;
	case BT_IMME_SEND:
		if (!sendBtUartData((uint8_t*) "AT+IMME1\n\r", sizeof((uint8_t*) "AT+IMME1\n\r"))) {
			timeout = getBTSystick();
			initState = BT_IMME_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_IMME_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+Set:1")) {
				initState = BT_ROLE_MODE;
			}
		}
	break;
	case BT_ROLE_MODE:
		if (!sendBtUartData((uint8_t*) "AT+ROLE1\n\r", sizeof((uint8_t*) "AT+ROLE1\n\r"))) {
			timeout = getBTSystick();
			initState = BT_ROLE_MODE_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_ROLE_MODE_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+Set:1")) {
				initState = BT_MODE;
			}
		}
	break;
	case BT_MODE:
		if (!sendBtUartData((uint8_t*) "AT+MODE1\n\r", sizeof((uint8_t*) "AT+MODE1\n\r"))) {
			timeout = getBTSystick();
			initState = BT_MODE_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_MODE_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+Set:1")) {
				initState = BT_SHOW_SEND;
			}
		}
	break;
	case BT_SHOW_SEND:
		if (!sendBtUartData((uint8_t*) "AT+SHOW1\n\r", sizeof((uint8_t*) "AT+SHOW1\n\r"))) {
			timeout = getBTSystick();
			initState = BT_SHOW_SEND_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_SHOW_SEND_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+Set:1")) {
				initState = BT_RESET_SEND;
			}
		}
	break;
	case BT_RESET_SEND:
		if (!sendBtUartData((uint8_t*) "AT+RESET\n\r", sizeof((uint8_t*) "AT+RESET\n\r"))) {
			timeout = getBTSystick();
			initState = BT_RESET_SEND_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_RESET_SEND_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+RESET")) {
				initState = BT_START_SEND;
			}
		}
	break;
	case BT_START_SEND:
		if (!sendBtUartData((uint8_t*) "AT+START\n\r", sizeof((uint8_t*) "AT+START\n\r"))) {
			timeout = getBTSystick();
			initState = BT_START_SEND_WAIT;
			waitResponseTimeout = 300;
		}
	break;
	case BT_START_SEND_WAIT:
		if (getBTRecvCompleted()) {
			if (findBTATCommandResp("OK+START")) {
				initState = EXIT;
			}
		}
	break;
	case EXIT:
	default:
		m_btSerialConnState = CONNECTED;
		timeout = 0;
		retry = 0;
		initState = BT_AT_SEND;
		m_btConfigState = BT_CONFIG_FINISH;
	break;
	}

	if (getBTSystick() - timeout > HAL_TIMEOUT_UNIT1MS(waitResponseTimeout) && retry < 3) {
		m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		initState = BT_AT_SEND;
		retry++;
	}
	else if (retry >= 3) {
		m_btConnState = m_btSerialConnState = DISCONNECTED;
		timeout = 0;
		retry = 0;
		initState = BT_AT_SEND;
		m_btConfigState = BT_CONFIG_TIMEOUT;
	}

}

uint8_t sendBtUartData(uint8_t *data, uint8_t len) {
	clearBtRxRawBuffCnt();
	if ((HAL_UART_Transmit(m_btUart, data, len, 15)) != HAL_OK) {
		return 1;
	}
	return 0;
}

void btInit(void *uart) {
	m_btUart = (UART_HandleTypeDef*) uart;
	BT_Virtual_Rx_IT();
}

uint8_t getBtConnState(void) {
	return (uint8_t) m_btConnState;
}

void btConnection(void) {

}

void checkBtConnState(void) {

	if (getBtConnState() == CONNECTED) {
		return;
	}

	if (getBtSerialConnState() != CONNECTED) {
		btInitConfig();
		return;
	}

	//TODO: bluetooth cihazı ile bağlantı işlemini burada yap.

}

void btSearchBTCommandResponse(void) {
	if (getBTRecvCompleted() == BT_RECV_COMPLETED_IDLE && getBtConnState() != CONNECTED) {
		return;
	}

	//TODO: burada bt connected, reset, gelen mesaj vb. gibi kontrol işlemleri yapılacak.
}

void btControl(void) {
	if (m_btRxIterror != HAL_OK) {
		BT_Virtual_Rx_IT();
	}

	if (getBtConfigState() != BT_CONFIG_FINISH) {
		btInitConfig();
	}

	if (getBtConnectionState() != BT_CONNECTION_FINISH) {
		btConnection();
	}

	checkBtConnState();

	btSearchBTCommandResponse();
}

