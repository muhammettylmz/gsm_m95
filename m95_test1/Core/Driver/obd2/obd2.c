/*
 * obd2.c
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */

#include "obd2.h"
#include "obd2pids.h"
#include "bluetooth.h"

typedef enum {
	OBD_CONFIG_START, OBD_CONFIG_FINISH, OBD_CONFIG_TIMEOUT
} obdConfigState_e;

obdConfigState_e m_obdConfigState = OBD_CONFIG_START;
uint32_t m_obdSystickCnt;
uint8_t m_obdReConnectedFlag = 0;

void setObdConfigState(obdConfigState_e state) {
	m_obdConfigState = state;
}

uint8_t getObdConfigState(void) {
	return (uint8_t) m_obdConfigState;
}

uint32_t getOBDSystick(void) {
	return m_obdSystickCnt;
}

void obd2Init(void) {
	if(getBTRecvCompleted()){

	}
}

void obd2Control(void) {
	if (!getBtConnState()) {
		if (getObdConfigState() == OBD_CONFIG_FINISH) {
			m_obdReConnectedFlag = 1;
		}
		return;
	}

	if (m_obdReConnectedFlag || (getObdConfigState() != OBD_CONFIG_FINISH)) {
		m_obdReConnectedFlag = 0;
		obd2Init();
	}
}

void OBD_Virtual_Systick_Handler(void) {
	m_obdSystickCnt++;
}

