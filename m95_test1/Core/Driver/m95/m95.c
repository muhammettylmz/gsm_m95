/*
 * m95.c
 *
 *  Created on: Jul 30, 2023
 *      Author: muham
 */

#include "m95.h"
#include "main.h"
#include <string.h>
#include "mqtt.h"
#include "uart_debug.h"

/*timer cnt ~100us de bir artacak şekilde ayarlandı*/
#define TIMER_TIMEOUT_UNIT100US(x)		(x*100)
#define TIMER_TIMEOUT_UNIT1MS(x)	    (x*10)
#define _10MS							TIMER_TIMEOUT_UNIT1MS(10)
#define _5MS							TIMER_TIMEOUT_UNIT1MS(5)

UART_HandleTypeDef *m_uart;
uint8_t rxGSMRaw[512];
uint8_t rxGSMByte;
uint16_t rxBufferCnt;
uint32_t m_systick;
uint64_t m_timerCnt;
HAL_StatusTypeDef m_gsmRecvITError;

uint64_t m_uartRecvTimeoutStart;
uint8_t m_uartRecvCompleted;
moduleCfgState_e m_moduleConfigState;

void clearUartBuffer(void);

moduleCfgState_e getModuleConfigState(void) {
	return m_moduleConfigState;
}

void setModuleConfigState(moduleCfgState_e state) {
	m_moduleConfigState = state;
}

uint8_t getRecvCompleted(void) {
	return m_uartRecvCompleted;
}

uint32_t getSystickCnt(void) {
	return m_systick;
}

uint64_t getRecvTimeoutCnt(void) {
	return m_uartRecvTimeoutStart;
}

uint64_t getTimerCnt(void) {
	return m_timerCnt;
}

void startRecvTimeout(void) {
	m_uartRecvTimeoutStart = getSystickCnt();
}

void stopRecvTimeout(void) {
	m_uartRecvTimeoutStart = 0;
}

uint8_t checkRecvTimeout(void) {
	if ((getSystickCnt() - getRecvTimeoutCnt()) >= 10) {
		m_uartRecvCompleted = 1;
		stopRecvTimeout();
		return 1;  // timeout
	}
	return 0;  // not timeout
}

uint8_t getRecvTimeoutState(void) {
	return m_uartRecvTimeoutStart != 0;
}

/**
 * @brief GSM init copy UART handle
 * @retval None
 */
void gsmInit(void* uart){
	m_uart = (UART_HandleTypeDef*) uart;
	GSM_Virtual_Rx_IT();
	powerOff();
	powerOn();
}

/**
 * @brief GSM Module power on. (quectel M95 click module)
 * @retval None
 */
void powerOn(void) {
	HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_SET);
	uint32_t prevtimeout = getSystickCnt();
	// wait until state pin high level
	while (HAL_GPIO_ReadPin(STAT_M95_GPIO_Port, STAT_M95_Pin) != GPIO_PIN_SET) {
		HAL_Delay(1);
		if ((getSystickCnt() - prevtimeout) >= 800) {
			customDebugMsg("GSM Power on Timeout...\r\n");
			break;
		}
	}
	customDebugMsg("GSM Module power on\r\n");
	//state pin high level, pwrkey pin low level
	HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_RESET);
	HAL_Delay(1000);
}

/**
 * @brief GSM Module power off. (quectel M95 click module)
 * @retval None
 */
void powerOff(void){
	if(!sendATCommand((const uint8_t*)"AT+QPOWD=0\r\n")){
		customDebugMsg("GSM Module Power off...\r\n");
		HAL_Delay(500);
	}
}

uint32_t gsmConfigPrevTick = 0;
/**
 * @brief GSM Module configuration. Use after powerOn function.
 * @retval None
 */
void gsmConfig(void) {

	typedef enum {
		ECHO_MODE,
		ECHO_MODE_WAIT,
		STRING_TYPE,
		STRING_TYPE_WAIT,
		CPIN_READ,
		CPIN_READ_WAIT,
		CREG_READ,
		CREG_READ_WAIT,
		CGATT_READ,
		CGATT_READ_WAIT,
		REGISTER_TCP_IP,
		REGISTER_TCP_IP_WAIT,
		ACTIVE_GPRS,
		ACTIVE_GPRS_WAIT,
		CREG_ACTIVE,
		CREG_ACTIVE_WAIT,
		CGATT_ATTACH,
		CGATT_ATTACH_WAIT,
		EXIT,
	} module_cfg_e;

	static module_cfg_e state = ECHO_MODE;
	static uint8_t retry = 0;

	if (gsmConfigPrevTick == 0) {
		gsmConfigPrevTick = getSystickCnt();
	}

	m_moduleConfigState = MODULE_CONFIG_START;

	switch (state) {
	case ECHO_MODE: {
		//echo mode off
		if (!sendATCommand((const uint8_t*) "ATE0\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case ECHO_MODE_WAIT: {
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case STRING_TYPE: {
		// string error type
		if (!sendATCommand((const uint8_t*) "AT+CMEE=2\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case STRING_TYPE_WAIT: {
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case CPIN_READ: {
		if (!sendATCommand((const uint8_t*) "AT+CPIN?\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case CPIN_READ_WAIT: {
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "READY")) {
			state++;
		}
		break;
	}
	case CREG_READ: {
		if (!sendATCommand((const uint8_t*) "AT+CREG?\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case CREG_READ_WAIT: {
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "+CREG: 0,1")
					|| findATCommandResp((uint8_t*) "+CREG: 0,5")) {
				state++;
			}
			else {
				state = CREG_ACTIVE;
			}
		}
		break;
	}
	case CREG_ACTIVE: {
		if (!sendATCommand((const uint8_t*) "AT+CREG=1\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case CREG_ACTIVE_WAIT: {
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "OK")) {
				state = CGATT_READ;
			}
		}
		break;
	}
	case CGATT_READ: {
		if (!sendATCommand((const uint8_t*) "AT+CGATT?\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case CGATT_READ_WAIT: {
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "+CGATT: 1")) {
				state++;
			}
			else {
				state = CGATT_ATTACH;
			}
		}
		break;
	}
	case CGATT_ATTACH: {
		if (!sendATCommand((const uint8_t*) "AT+CGATT=1\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case CGATT_ATTACH_WAIT: {
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state = REGISTER_TCP_IP;
		}
		break;
	}
	case REGISTER_TCP_IP: {
		if (!sendATCommand((const uint8_t*) "AT+QIREGAPP\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case REGISTER_TCP_IP_WAIT: {
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "OK")) {

				state++;
			}
			else {  //error;
				state++;
			}
		}
		break;
	}
	case ACTIVE_GPRS: {
		if (!sendATCommand((const uint8_t*) "AT+QIACT\r\n")) {
			state++;
			gsmConfigPrevTick = getSystickCnt();
		}
		break;
	}
	case ACTIVE_GPRS_WAIT: {
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "OK")) {

				state = EXIT;
			}
			else {
				state = EXIT;
			}
		}

		break;
	}
		//fallt
	case EXIT:
	default:
		gsmConfigPrevTick = 0;
		retry = 0;
		state = ECHO_MODE;
		m_moduleConfigState = MODULE_CONFIG_FINISH;
		customDebugMsg("GSM Config SUCCESS... \r\n");
	break;
	}

	// komutların cevabı gelmez ise kontrol mekanizması konuldu.
	if ((getSystickCnt() - gsmConfigPrevTick) >= 500 && retry < 3) {
		state = ECHO_MODE;
		retry++;
		gsmConfigPrevTick = getSystickCnt();
	}
	else if (retry >= 3) {
		gsmConfigPrevTick = 0;
		retry = 0;
		state = ECHO_MODE;
		m_moduleConfigState = MODULE_CONFIG_TIMEOUT;
		customDebugMsg("GSM Config TIMEOUT... \r\n");
		// config module error
	}
//	else {
//		HAL_Delay(5);
//	}
}

/**
 * @brief monitoring GSM Module power. if power off, module power on state. (quectel M95 click module)
 * @retval None
 */
void monitoringPowerOff(void) {
	if (HAL_GPIO_ReadPin(STAT_M95_GPIO_Port, STAT_M95_Pin) != GPIO_PIN_SET) {
		HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_SET);
		setModuleConfigState(MODULE_CONFIG_START);
		setMQTTConfigState(MQTT_CONFIG_START);
		setMQTTOpenState(MQTT_NOT_OPEN);
		setMQTTConnectState(MQTT_NOT_CONNECT);
	}
	else {
		HAL_GPIO_WritePin(PWRKEY_GPIO_Port, PWRKEY_Pin, GPIO_PIN_RESET);
	}
}

/**
 * @brief clear uart buffer
 * @retval None
 */
void clearUartBuffer(void) {
	rxBufferCnt = 0;
	m_uartRecvCompleted = 0;
	memset(rxGSMRaw, 0, sizeof(rxGSMRaw));
}

/**
 * @brief Send specific AT command
 * @retval success 0 , others 1
 */
uint8_t sendATCommand(const uint8_t *commad) {
	clearUartBuffer();
	if (HAL_UART_Transmit(m_uart, commad, strlen((const char*) commad), 40) != HAL_OK) {
		return 1;
	}
	return 0;
}

/**
 * @brief AT Komut cevabının bulunması, recv completed olduğunda doğru cevabı verecek
 * @retval succes 0 , others 1
 */
uint8_t findATCommandResp(uint8_t *resp) {
	return strstr((char*) rxGSMRaw, (char*) resp) != NULL;
}

/**
 * @brief Virtual HAL Receive IT
 * @retval None
 */
void GSM_Virtual_Rx_IT(void) {
	m_gsmRecvITError = HAL_UART_Receive_IT(m_uart, &rxGSMByte, 1);
}

/**
 * @brief HAL Rx Callback virtual funciton
 * @retval None
 */
void GSM_Virtual_UART_RxCpltCallback(void *uart) {
	if (m_uart->Instance == ((UART_HandleTypeDef*) uart)->Instance) {
		rxGSMRaw[rxBufferCnt++] = rxGSMByte;
		GSM_Virtual_Rx_IT();
		startRecvTimeout();
	}
}

/**
 * @brief Uart üzerinden gsm e data gönderme
 * @retval 0 is ok , others 1
 */
uint8_t sendUartData(const uint8_t *data, uint16_t len) {
	uint16_t forCnt = len / 64;
	uint8_t leapCnt = len - (forCnt * 64);
	HAL_StatusTypeDef err = HAL_OK;

	clearUartBuffer();

	for (uint8_t u8 = 0; u8 < forCnt; u8++) {
		err = HAL_UART_Transmit(m_uart, &data[u8 * 64], 64, 30);
	}

	if (leapCnt) {
		err = HAL_UART_Transmit(m_uart, &data[forCnt * 64], leapCnt, 30);
	}

	return err != HAL_OK;
}
/**
 * @brief Virtual Timer elapsed callback function, ~100us
 * @retval None
 */
void GSM_Virtual_TIM_ElapsedCallback(void *tim) {
	(void) tim;
	m_timerCnt++;
}

/**
 * @brief Virtual Systick Handler
 * @retval None
 */
void GSM_Virtual_Systick(void) {
	m_systick++;
	if (getRecvTimeoutState()) {
		checkRecvTimeout();
	}
}

void getRxGSMRawData(uint8_t *data) {
	memcpy(data, rxGSMRaw, rxBufferCnt);
}

void gsmControl(void) {
	monitoringPowerOff();
	if(m_gsmRecvITError != HAL_OK){
		GSM_Virtual_Rx_IT();
	}
	if (getModuleConfigState() != MODULE_CONFIG_FINISH) {
		gsmConfig();
		return;
	}


	/*
	 * */
}

/*
 SMS
 AT+CMGF=1 //Set SMS message format as text mode
 OK
 AT+CSCS="GSM" //Set character set as GSM which is used by the TE
 OK
 AT+CMGW="phone number"
 > This is a test from Quectel //Enter in text, 0x1A <CTRL+Z> write message, 0x1B<ESC> quits  without sending

 * */
