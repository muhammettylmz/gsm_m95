/*
 * vehicleTrackingSystem.c
 *
 *  Created on: Aug 28, 2023
 *      Author: muham
 */

/* Includes ------------------------------------------------------------------*/
#include "vehicleTrackingSystem.h"
#include "main.h"
#include "m95.h"
#include "mqtt.h"
#include "uart_debug.h"
#include "gps.h"
#include "MY_LIS3DSH.h"
#include "bluetooth.h"
#include "obd2.h"
#include <stdbool.h>

/* External variables --------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;
extern IWDG_HandleTypeDef hiwdg;

uint32_t m_vtsButtonPressedCnt;
uint32_t m_vtsButtonReleaseCnt;

uint8_t btnState = 0;
uint8_t isRls = 1;

iwdgRefreshRequestType_e m_prevIwdgRequestType;
iwdgRefreshRequestType_e m_currentIwdgRequestType;

/**
 * @brief STM32 IWDG refresh counter
 * @retval None
 */
void iwdgControl(void) {
	HAL_IWDG_Refresh(&hiwdg);
}

/**
 * @brief Virtual IWDG Refresh.
 * @retval None
 */
void virtualIwdgRefresh(iwdgRefreshRequestType_e requestType) {
	m_prevIwdgRequestType = requestType;
	m_currentIwdgRequestType = requestType;
	//TODO: ileride bu requesttype kullanılacak.
	iwdgControl();
}

/**
 * @brief STM32 Disco user button press and release check
 * @retval None
 */
void checkButton(uint8_t *btn, uint8_t *rls) {
	static uint16_t pinSetCnt = 0;
	static uint16_t pinResetCnt = 0;

	//debounce cnt
	if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) {
		pinSetCnt++;
		pinResetCnt = 0;
	}
	else {
		pinSetCnt = 0;
		pinResetCnt++;
	}

	// button pressed
	if (pinSetCnt >= TIMER_TIMEOUT_UNIT1MS(15)) {
		pinSetCnt = 0;
		if (*btn == 0) {
			*rls = 1;
			m_vtsButtonPressedCnt++;
		}
		*btn = 1;
	}
	//button released
	else if (pinResetCnt >= TIMER_TIMEOUT_UNIT1MS(25)) {
		pinResetCnt = 0;
		if (*btn == 1) {
			*rls = 0;
			m_vtsButtonReleaseCnt++;
		}
		*btn = 0;
	}
}

/**
 * @brief Vehicle Tracking System subsystem inits
 * @retval None
 */
void VTSInit(void) {

	uartDebugInit(&huart6);
	customDebugMsg("Start STM32F407 disco : %d , %s%d.%d\r\n", 12, "Vecihle Tracking V", 1, 2);

	HAL_TIM_Base_Start_IT(&htim6);
	memsInit(&hspi1);
	btInit(&huart5);
	gpsInit(&huart4);
	gsmInit(&huart3);
	//obd2Init();
	customDebugMsg("Enter infinite While loop... \r\n");
}

/**
 * @brief Vehicke Tracking System Control infinite loop
 * @retval None
 */
void VTSControl(void) {

	while (true) {
		gsmControl();
		mqttControl();
		gpsControl();
		memsControl();
		btControl();
		obd2Control();
		iwdgControl();
	}
}

/**
 * @brief Virtual Systick Handler
 * @retval None
 */
void VTS_Virtual_SysTick_Handler(void) {
	GSM_Virtual_Systick();
	GPS_Virtual_Systick();
	BT_Virtual_Systick();
	MQTT_Virtual_Systick_Handler();
	MEMS_Virtual_Systick_Handler();
	OBD_Virtual_Systick_Handler();
}

/**
 * @brief Virtual UART Rx Completed Callback
 * @retval None
 */
void VTS_Virtual_UART_RxCpltCallback(void *huart) {
	GSM_Virtual_UART_RxCpltCallback(huart);
	GPS_Virtual_UART_RxCpltCallback(huart);
	BT_Virtual_UART_RxCpltCallback(huart);
	DEBUG_Virtual_UART_RxCpltCallback(huart);
}

/**
 * @brief Virtual TIM Period Elapsed Callback ~100us
 * @retval None
 */
void VTS_Virtual_TIM_PeriodElapsedCallback(void *htim) {
	GSM_Virtual_TIM_ElapsedCallback(htim);
	MQTT_Virtual_TIM_ElapsedCallback(htim);
	checkButton(&btnState, &isRls);
}

/**
 * @brief Virtual GPIO EXTI Callback
 * @retval None
 */
void VTS_Virtual_GPIO_EXTI_Callback(uint16_t pin) {
	if (pin == MEMS_INT2_Pin) {
		MEMS_Virtual_GPIO_EXTI();
	}
}
