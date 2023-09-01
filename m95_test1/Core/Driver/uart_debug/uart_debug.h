/*
 * uart_debug.h
 *
 *  Created on: Aug 9, 2023
 *      Author: muham
 */

#ifndef DRIVER_UART_DEBUG_UART_DEBUG_H_
#define DRIVER_UART_DEBUG_UART_DEBUG_H_

extern void uartDebugInit(void* uart);
extern int customDebugMsg(const char* format , ...);
extern void memsDebugAcc(const char* format , ...);
extern void nmeaDebugUart(uint8_t data);
extern void obd2DebugUart(uint8_t* data, uint8_t len);

extern void DEBUG_Virtual_UART_RxCpltCallback(void* uart);

#endif /* DRIVER_UART_DEBUG_UART_DEBUG_H_ */
