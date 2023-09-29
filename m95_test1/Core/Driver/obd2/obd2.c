/*
 * obd2.c
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */

#include "obd2.h"
#include "obd2pids.h"
#include "bluetooth.h"

static uint32_t m_obdSystickCnt;

//static uint32_t getObdSystickCnt(void){
//	return m_obdSystickCnt;
//}


void obd2Init(void){
//	if(){
//
//	}
}

void obd2Control(void){

}

void OBD_Virtual_Systick_Handler(void){
	m_obdSystickCnt++;
}


