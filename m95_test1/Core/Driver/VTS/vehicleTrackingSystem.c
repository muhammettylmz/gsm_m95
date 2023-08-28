/*
 * vehicleTrackingSystem.c
 *
 *  Created on: Aug 28, 2023
 *      Author: muham
 */

/* Includes ------------------------------------------------------------------*/
#include <vehicleTrackingSystem.h>
#include "main.h"
#include "m95.h"
#include "mqtt.h"
#include "uart_debug.h"
#include "gps.h"
#include "MY_LIS3DSH.h"
#include "bluetooth.h"

/* External variables --------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;


uint8_t btnState = 0;
uint8_t isRls = 1;

/**
 * @brief STM32 Disco user button press and release check
 * @retval None
 */
void checkButton(uint8_t *btn, uint8_t *rls) {
	GPIO_PinState state = GPIO_PIN_RESET;
	//btn pressed
	state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
	if (state == GPIO_PIN_SET) {
		HAL_Delay(5);
		state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
		if (state == GPIO_PIN_SET) {
			HAL_Delay(7);
			state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
			if (state == GPIO_PIN_SET) {
				if (*btn == 0) {
					*rls = 1;
				}
				*btn = 1;
				return;
			}
		}
	}

	//btn realesed
	if (state == GPIO_PIN_RESET) {
		HAL_Delay(5);
		state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
		if (state == GPIO_PIN_RESET) {
			HAL_Delay(7);
			state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
			if (state == GPIO_PIN_RESET) {
				if (*btn == 1) {
					*rls = 0;
				}
				*btn = 0;
				return;
			}
		}
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

	customDebugMsg("Enter infinite While loop... \r\n");
}

/**
 * @brief Vehicke Tracking System Control infinite loop
 * @retval None
 */
void VTSControl(void) {

	while (1) {
		gsmControl();
		mqttControl();
		gpsControl();
		memsControl();
		btControl();

		checkButton(&btnState, &isRls);
		if (btnState && isRls) {
			customDebugMsg("Button is pressed...\r\nPreparing Send MQTT Publish message\r\n");
			setMQTTPublishReadyState(PUBLISH_READY);
			isRls = 0;
		}
		HAL_Delay(2);
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
}

/**
 * @brief Virtual UART Rx Completed Callback
 * @retval None
 */
void VTS_Virtual_UART_RxCpltCallback(void *huart) {
	GSM_Virtual_UART_RxCpltCallback(huart);
	GPS_Virtual_UART_RxCpltCallback(huart);
	BT_Virtual_UART_RxCpltCallback(huart);
}

/**
 * @brief Virtual TIM Period Elapsed Callback
 * @retval None
 */
void VTS_Virtual_TIM_PeriodElapsedCallback(void *htim) {
	GSM_Virtual_TIM_ElapsedCallback(htim);
	MQTT_Virtual_TIM_ElapsedCallback(htim);
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
