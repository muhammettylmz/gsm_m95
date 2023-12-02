/*
 * obd2.h
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */

#ifndef DRIVER_OBD2_OBD2_H_
#define DRIVER_OBD2_OBD2_H_


typedef struct {
	uint8_t		calcEngineLoadValue;//04
	int8_t		engineCoolantTemp;//05
	uint16_t    fuelPressure;	// 0A
	float 		engineRPM;		// 0C
	uint8_t 	fuelLevelInput; // 2F
	int8_t		engineOilTemp;  // 5C
	float 		engineFuelRate; // 5E
	uint8_t     vehicleSpeed;	// 0D
	uint32_t    vehicleOdometer;//A6
	char 		vehicleDtcData[3][6];
	uint8_t 	vehicleDtcArrIndex;
	uint8_t		obd2SocketConnected;
}obd2VehicleData_t;

typedef enum{
	OBD2_PERIODIC_DATA_IDLE,
	OBD2_PERIODIC_DATA_COMPLETED
}obd2PeriodicDataCompletedState_e;

//extern void OBD_Virtual_TIM_ElapsedCallback(void* tim);
extern void OBD_Virtual_Systick_Handler(void);
extern void OBD_Virtual_Rx_Completed_Callback(unsigned char rxData);

extern void obd2Init(void);
extern void obd2Control(void);
extern obd2VehicleData_t getPeriodicObdVehicleData(void);
extern obd2PeriodicDataCompletedState_e getObd2PeroidicDataCompletedState(void);
extern void setObd2PeroidicDataCompletedState(obd2PeriodicDataCompletedState_e state);


#endif /* DRIVER_OBD2_OBD2_H_ */
