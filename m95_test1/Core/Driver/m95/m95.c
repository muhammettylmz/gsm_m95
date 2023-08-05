/*
 * m95.c
 *
 *  Created on: Jul 30, 2023
 *      Author: muham
 */

#include "m95.h"
#include "main.h"
#include <string.h>
#include "mqtt.h"


/*timer cnt ~100us de bir artacak şekilde ayarlandı*/
#define TIMER_TIMEOUT_UNIT100US(x)		(x*100)
#define TIMER_TIMEOUT_UNIT1MS(x)	    (x*10)
#define _5MS							TIMER_TIMEOUT_UNIT1MS(5)

UART_HandleTypeDef *m_uart;
uint8_t rxGSMRaw[512];
uint8_t rxGSMByte;
uint16_t rxBufferCnt;
uint32_t m_systick;
uint64_t m_timerCnt;

uint64_t m_uartRecvTimeoutStart;
uint8_t m_uartRecvCompleted;

void clearUartBuffer(void);

uint8_t getRecvCompleted(void) {
	return m_uartRecvCompleted;
}

uint32_t getSystickCnt(void) {
	return m_systick;
}

uint64_t getRecvTimeoutCnt(void) {
	return m_uartRecvTimeoutStart;
}

uint64_t getTimerCnt(void) {
	return m_timerCnt;
}

void startRecvTimeout(void) {
	m_uartRecvTimeoutStart = getTimerCnt();
}

void stopRecvTimeout(void) {
	m_uartRecvTimeoutStart = 0;
}

uint8_t checkRecvTimeout(void) {
	if ((getTimerCnt() - getRecvTimeoutCnt()) >= _5MS) {
		m_uartRecvCompleted = 1;
		stopRecvTimeout();
		return 1; // timeout
	}
	return 0; // not timeout
}

uint8_t getRecvTimeoutState(void) {
	return m_uartRecvTimeoutStart != 0;
}

/**
 * @brief Copy pointer GSM Module uart handle
 * @retval None
 */
void moveUart(void *uart) {
	m_uart = (UART_HandleTypeDef*) uart;
}

/**
 * @brief GSM Module power on. (quectel M95 click module)
 * @retval None
 */
void powerOn(void) {
	HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_SET);
	// wait until state pin high level
	while (HAL_GPIO_ReadPin(STAT_M95_GPIO_Port, STAT_M95_Pin) != GPIO_PIN_SET) {
		HAL_Delay(1);
	}
	//state pin high level, pwrkey pin low level
	HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_RESET);
}

/**
 * @brief GSM Module configuration. Use after powerOn function.
 * @retval None
 */
void moduleConfig(void) {

#define ECHO_MODE 0
#define ECHO_MODE_WAIT 1
#define STRING_TYPE 2
#define STRING_TYPE_WAIT 3
//#define test_secwrite 4
//#define test_secwrite_wait 5
//#define test_secwrite_file_write 6
//#define test_secwrite_file_write_wait 7

	uint8_t whileState = 1;
	uint8_t state = ECHO_MODE;

	uint32_t prevtimeout = getSystickCnt();
	uint8_t retry = 0;

//	HAL_StatusTypeDef err = HAL_OK;

	while (whileState) {
		switch (state) {
		case ECHO_MODE: {
			if (!sendATCommand((const uint8_t*) "ATE0\r\n")) {
				state++;
			}
			break;
		}
		case ECHO_MODE_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case STRING_TYPE: {
			if (!sendATCommand((const uint8_t*) "AT+CMEE=2\r\n")) {
				state++;
			}
			break;
		}
		case STRING_TYPE_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		/*case test_secwrite: {
			if (!sendATCommand((const uint8_t*) "AT+QSECWRITE=\"RAM:cacert.pem\",1188,200\r\n")) {
				state++;
			}
			break;
		}
		case test_secwrite_wait: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "CONNECT")) {
				state++;
			} else if (getRecvCompleted() && findATCommandResp((uint8_t*) "ERROR")) {
				continue;
			}
			break;
		}
		case test_secwrite_file_write: {
			clearUartBuffer();
			for (uint8_t u8 = 0; u8 < 18; u8++) {
				err = HAL_UART_Transmit(m_uart, &awsRootCA1[0 + u8 * 64], 64, 100);
			}

			err = HAL_UART_Transmit(m_uart, &awsRootCA1[64 * 18], 36, 100);
			if (err == HAL_OK) {
				state++;
			}
			break;
		}
		case test_secwrite_file_write_wait: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "+QSECWRITE")) {
				if (findATCommandResp((uint8_t*) "OK")) {
					state++;
				}
			}
			break;
		}*/
		default:
			whileState = 0;
			break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getSystickCnt() - prevtimeout) >= 1000 && retry < 3) {
			state = 0;
			retry++;
		} else if (retry >= 3) {
			whileState = 0;
			// config module error
		} else {
			HAL_Delay(5);
		}
	} // while end
}

/**
 * @brief monitoring GSM Module power. if power off, module power on state. (quectel M95 click module)
 * @retval None
 */
void monitoringPowerOff(void) {
	if (HAL_GPIO_ReadPin(STAT_M95_GPIO_Port, STAT_M95_Pin) != GPIO_PIN_SET) {
		HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_RESET);
	}
}

/**
 * @brief clear uart buffer
 * @retval None
 */
void clearUartBuffer(void) {
	rxBufferCnt = 0;
	m_uartRecvCompleted = 0;
	memset(rxGSMRaw, 0, sizeof(rxGSMRaw));
}

/**
 * @brief Send specific AT command
 * @retval success 0 , others 1
 */
uint8_t sendATCommand(const uint8_t *commad) {
	clearUartBuffer();
	if (HAL_UART_Transmit(m_uart, commad, strlen((const char*) commad), 40) != HAL_OK) {
		return 1;
	}
	return 0;
}

/**
 * @brief AT Komut cevabının bulunması, recv completed olduğunda doğru cevabı verecek
 * @retval succes 0 , others 1
 */
uint8_t findATCommandResp(uint8_t *resp) {
	return strstr((char*) rxGSMRaw, (char*) resp) != NULL;
}

/**
 * @brief Virtual HAL Receive IT
 * @retval None
 */
void GSM_Virtual_Rx_IT(void) {
	HAL_UART_Receive_IT(m_uart, &rxGSMByte, 1);
}

/**
 * @brief HAL Rx Callback virtual funciton
 * @retval None
 */
void GSM_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_uart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		rxGSMRaw[rxBufferCnt++] = rxGSMByte;
		GSM_Virtual_Rx_IT();
		startRecvTimeout();
	}
}

/**
 * @brief Virtual Timer elapsed callback function, ~100us
 * @retval None
 */
void GSM_Virtual_TIM_ElapsedCallback(void *tim) {
	(void) tim;
	m_timerCnt++;
	if (getRecvTimeoutState()) {
		checkRecvTimeout();
	}
}

/**
 * @brief Virtual Systick Handler
 * @retval None
 */
void GSM_Virtual_Systick(void) {
	m_systick++;
}

/*
 SMS
 AT+CMGF=1 //Set SMS message format as text mode
 OK
 AT+CSCS="GSM" //Set character set as GSM which is used by the TE
 OK
 AT+CMGW="phone number"
 > This is a test from Quectel //Enter in text, 0x1A <CTRL+Z> write message, 0x1B<ESC> quits  without sending

 * */
