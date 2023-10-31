/*
 * bluetooth.h
 *
 *  Created on: Aug 19, 2023
 *      Author: muham
 */

#ifndef DRIVER_BLUETOOTH_BLUETOOTH_H_
#define DRIVER_BLUETOOTH_BLUETOOTH_H_

#include <stdint.h>

typedef enum {
	DISCONNECTED, CONNECTED
} connState_e;


extern void BT_Virtual_Systick(void);
extern void BT_Virtual_Rx_IT(void);
extern void BT_Virtual_UART_RxCpltCallback(void *uart);

extern uint8_t sendBtUartData(uint8_t* data, uint8_t len);
extern uint8_t getBtConnState(void);
extern uint8_t getBtSerialConnState(void);
extern uint8_t getBTRecvCompleted(void);
extern uint8_t getBTStatePinState(void);
extern uint8_t getBTModePin(void);
extern void setBtSerialConnState(uint8_t state);
extern void setBTModePin(uint8_t state);


extern void btInit(void* uart);
extern void btControl(void);

#endif /* DRIVER_BLUETOOTH_BLUETOOTH_H_ */
