/*
 * bluetooth.h
 *
 *  Created on: Aug 19, 2023
 *      Author: muham
 */

#ifndef DRIVER_BLUETOOTH_BLUETOOTH_H_
#define DRIVER_BLUETOOTH_BLUETOOTH_H_

#include <stdint.h>

extern void BT_Virtual_Systick(void);
extern void BT_Virtual_Rx_IT(void);
extern void BT_Virtual_UART_RxCpltCallback(void *uart);

extern uint8_t sendBtUartData(uint8_t* data, uint16_t len);
extern uint8_t getBtConnState(void);

extern void btInit(void* uart);
extern void btControl(void);
extern uint8_t getBtSerialConnState(void);

#endif /* DRIVER_BLUETOOTH_BLUETOOTH_H_ */
