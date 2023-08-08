/*
 * mqtt.h
 *
 *  Created on: Jul 31, 2023
 *      Author: muham
 */

#ifndef DRIVER_MQTT_MQTT_H_
#define DRIVER_MQTT_MQTT_H_

#include <stdint.h>
#include <string.h>
#include "cert.h"

#define MQTT_AWS_URL  	(uint8_t*)"a16f5x7vu3zfui-ats.iot.eu-central-1.amazonaws.com"
#define MQTT_AWS_PORT 	8883
#define MQTT_CLIENT		(uint8_t*)"yehhep"
#define MQTT_AWS_TOPIC	(const char*)"yehhep/%d/status/"

typedef enum {
	MQTT_CONFIG_START,
	MQTT_CONFIG_FINISH,
	MQTT_CONFIG_TIMEOUT,
}mqttConfigState_e;

typedef enum{
	MQTT_NOT_CONNECT,
	MQTT_CONNECTED,
	MQTT_CONNECT_TIMEOUT
}mqttConnectState_e;

typedef enum{
	MQTT_NOT_OPEN,
	MQTT_OPEN_SUCCESS,
	MQTT_OPEN_TIMEOUT
}mqttOpenState_e;

typedef enum{
	MQTT_PUBLISH_IDLE,
	MQTT_PUBLISH_SUCCESS,
	MQTT_PUBLISH_TIMEOUT
}mqttPubReqState_e;


extern void MQTT_Virtual_TIM_ElapsedCallback(void* tim);
extern void MQTT_Virtual_Systick_Handler(void);
extern void setMQTTConfigState(mqttConfigState_e state);
extern mqttConfigState_e getMQTTConfigState(void);
extern void mqttInit(void);
extern void mqttControl(void);

#endif /* DRIVER_MQTT_MQTT_H_ */
