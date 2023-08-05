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

#define MQTT_AWS_URL  	"endpoint"
#define MQTT_AWS_PORT 	8883
#define MQTT_CLIENT		"yehhep"
#define MQTT_AWS_TOPIC	"yehhep/+/status/"


extern void MQTT_Virtual_TIM_ElapsedCallback(void* tim);
extern void MQTT_Virtual_Systick_Handler(void);

#endif /* DRIVER_MQTT_MQTT_H_ */
