/*
 * uart_debug.c
 *
 *  Created on: Aug 9, 2023
 *      Author: muham
 */
#include "main.h"
#include "uart_debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define STDOUT_BUFFER_SIZE   512

UART_HandleTypeDef *m_debugUart;

void uartDebugInit(void* uart){
	m_debugUart = (UART_HandleTypeDef*)uart;
}

int customDebugMsg(const char* format , ...)
{
#ifdef CUSTOM_DEBUG
	char tmpBuf[STDOUT_BUFFER_SIZE] = {0};
	va_list arg = {};
	va_start(arg,format);

	int lenght = vsnprintf(tmpBuf,STDOUT_BUFFER_SIZE,format,arg);
	HAL_UART_Transmit(m_debugUart, (uint8_t*)tmpBuf, (uint16_t)lenght, 100);
	va_end(arg);

	return lenght;
#else
	return 0;
#endif
}

void memsDebugAcc(const char* format , ...){
	char tmpBuf[STDOUT_BUFFER_SIZE] = {0};
	va_list arg = {};
	va_start(arg,format);

	int lenght = vsnprintf(tmpBuf,STDOUT_BUFFER_SIZE,format,arg);
	HAL_UART_Transmit(m_debugUart, (uint8_t*)tmpBuf, (uint16_t)lenght, 100);
	va_end(arg);
}

void nmeaDebugUart(uint8_t data){
	static uint8_t write = 0;
	write = data;
	HAL_UART_Transmit(m_debugUart, &write, 1, 1);
}


