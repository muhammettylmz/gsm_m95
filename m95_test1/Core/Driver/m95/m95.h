/*
 * m95.h
 *
 *  Created on: Jul 30, 2023
 *      Author: muham
 */

#ifndef DRIVER_M95_M95_H_
#define DRIVER_M95_M95_H_

#include <stdint.h>

extern void GSM_Virtual_Systick(void);
extern void GSM_Virtual_Rx_IT(void);
extern void GSM_Virtual_UART_RxCpltCallback(void *uart);
extern void GSM_Virtual_TIM_ElapsedCallback(void* tim);
extern void moveUart(void *uart);
extern void powerOn(void);
extern void moduleConfig(void);
extern void monitoringPowerOff(void);
extern uint8_t sendATCommand(const uint8_t *commad);

#endif /* DRIVER_M95_M95_H_ */
