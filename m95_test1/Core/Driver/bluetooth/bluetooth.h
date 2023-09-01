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

extern void sendBtUartData(uint8_t* data, uint16_t len);

extern void btInit(void* uart);
extern void btControl(void);

#endif /* DRIVER_BLUETOOTH_BLUETOOTH_H_ */
