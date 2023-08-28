/*
 * vehicleTrackingSystem.h
 *
 *  Created on: Aug 28, 2023
 *      Author: muham
 */

#ifndef DRIVER_VTS_VEHICLETRACKINGSYSTEM_H_
#define DRIVER_VTS_VEHICLETRACKINGSYSTEM_H_

#include <stdint.h>

extern void VTSInit(void);
extern void VTSControl(void);

extern void VTS_Virtual_SysTick_Handler(void);
extern void VTS_Virtual_UART_RxCpltCallback(void *huart);
extern void VTS_Virtual_TIM_PeriodElapsedCallback(void *htim);
extern void VTS_Virtual_GPIO_EXTI_Callback(uint16_t pin);

#endif /* DRIVER_VTS_VEHICLETRACKINGSYSTEM_H_ */
