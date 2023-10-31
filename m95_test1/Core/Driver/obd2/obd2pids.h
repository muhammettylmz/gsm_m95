/*
 * obd2pids.h
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */

#ifndef DRIVER_OBD2_OBD2PIDS_H_
#define DRIVER_OBD2_OBD2PIDS_H_

typedef enum {
    PROTOCOL_AUTO = 0,
    PROTOCOL_ISO_9141_2 = 3,
    PROTOCOL_KWP2000_5KBPS = 4,
    PROTOCOL_KWP2000_FAST = 5,
    PROTOCOL_CAN_11B_500K = 6,
    PROTOCOL_CAN_29B_500K = 7,
    PROTOCOL_CAN_29B_250K = 8,
    PROTOCOL_CAN_11B_250K = 9,
} obd2Protocols_e;

typedef enum {
	SHOW_CURRENT_DATA_MODE = 1,
	SHOW_FREEZE_FRAME_DATA_MODE,
	SHOW_STORED_DTC_MODE,
	CLEAR_DTC_AND_STORED_VALUES_MODE,
	TEST_RESULT_OXY_SENSOR_MONITOR_MODE,
	SHOW_PENDING_DTC_MODE = 7
}obd2ServiceMode_e;

#define PIDs_FUEL_SYSTEM_STATUS 			"0103"
#define PIDs_FUEL_SYSTEM_STATUS_REP 		"4103"
#define PIDs_CALCULATE_ENGINE_LOAD 			"0104"
#define PIDs_CALCULATE_ENGINE_LOAD_REP 		"4104"
#define PIDs_ENGINE_COOLANT_TEMP 			"0105"
#define PIDs_ENGINE_COOLANT_TEMP_REP 		"4105"
#define PIDs_ODOMETER_VALUE 				"01A6"
#define PIDs_ODOMETER_VALUE_REP 			"41A6"

//typedef enum{
//	PIDs_SUPPORT_1_20 = 0, // response big-endian 4 byte
//	PIDs_DTC_STORED_DATA = 2, // resonse 2 byte, use only sevice mode 3
//	PIDs_FUEL_SYSTEM_STATUS = 3, // response 2 byte
//	PIDs_CALCULATE_ENGINE_LOAD = 4, // response 1 byte, response/2.55
//	PIDs_ENGINE_COOLANT_TEMP = 5, //response signed 1 byte, response - 40
//	PIDs_FUEL_PRESSURE = 10, // response 1 byte, response * 3
//	PIDs_MANIFOLD_ABSOLUTE_PRESSURE = 11, // response 1 byte
//	PIDs_ENGINE_SPEED = 12, // response 2 byte
//	PIDs_VEHICLE_SPEED = 13, // response 2 byte
//	PIDs_SUPPORT_21_40 = 32, // response big-endian 4 byte
//	PIDs_FUEL_LEVEL = 0x2F,
//	PIDs_SUPPORT_41_60 = 64, // response big-endian 4 byte
//	PIDs_ENGINE_FUEL_RATE = 0x5E,
//	PIDs_SUPPORT_61_80 = 96, // response big-endian 4 byte
//	PIDs_SUPPORT_81_A0 = 128, // response big-endian 4 byte
//	PIDs_SUPPORT_A1_C0 = 160, // response big-endian 4 byte
//	PIDs_ODOMETER_VALUE = 0xA6, // response 4 byte, (a*2^24 + b*a*2^16 + c*2^8 + d) /10
//	PIDs_SUPPORT_C1_E0 = 192, // response big-endian 4 byte
//}serviceModeStandartPIDs_e;

#endif /* DRIVER_OBD2_OBD2PIDS_H_ */
