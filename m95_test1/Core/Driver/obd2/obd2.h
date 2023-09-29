/*
 * obd2.h
 *
 *  Created on: Sep 29, 2023
 *      Author: muham
 */

#ifndef DRIVER_OBD2_OBD2_H_
#define DRIVER_OBD2_OBD2_H_

//extern void OBD_Virtual_TIM_ElapsedCallback(void* tim);
extern void OBD_Virtual_Systick_Handler(void);

extern void obd2Init(void);
extern void obd2Control(void);


#endif /* DRIVER_OBD2_OBD2_H_ */
