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

#endif /* DRIVER_UART_DEBUG_UART_DEBUG_H_ */
