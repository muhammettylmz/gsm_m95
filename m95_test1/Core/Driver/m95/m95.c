/*
 * m95.c
 *
 *  Created on: Jul 30, 2023
 *      Author: muham
 */

#include "m95.h"
#include "main.h"
#include <string.h>

UART_HandleTypeDef *m_uart;
uint8_t rxGSMRaw[1024];
uint8_t rxGSMByte;
uint16_t rxBufferCnt;
uint32_t m_systick;
uint64_t m_timerCnt;

//MQTT SSL cert and keys
uint8_t sslCA[1024];
uint8_t sslCC[1024];
uint8_t sslCK[1024];

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
	// ATE<value> --> value = 0 echo mode off , value = 1 echo mode on
	// AT+CMEE=2 --> error code with string type
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
	memset(rxGSMRaw, 0, sizeof(rxGSMRaw));
}

/**
 * @brief Send specific AT command
 * @retval None
 */
uint8_t sendATCommand(const uint8_t *commad) {
	clearUartBuffer();
	if (HAL_UART_Transmit(m_uart, commad, strlen((const char*) commad), 40)
			!= HAL_OK) {
		return 1;
	}
	return 0;
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
	}
}

/**
 * @brief Virtual Timer elapsed callback function
 * @retval None
 */
void GSM_Virtual_TIM_ElapsedCallback(void* tim){
	(void)tim;
	m_timerCnt++;
}

/**
 * @brief Virtual Systick Handler
 * @retval None
 */
void GSM_Virtual_Systick(void) {
	m_systick++;
}

