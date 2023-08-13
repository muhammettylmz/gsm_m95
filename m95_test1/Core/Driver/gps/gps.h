/*
 * gps.h
 *
 *  Created on: Aug 12, 2023
 *      Author: muham
 */

#ifndef DRIVER_GPS_GPS_H_
#define DRIVER_GPS_GPS_H_

extern void GPS_Virtual_Systick(void);
extern void GPS_Virtual_Rx_IT(void);
extern void GPS_Virtual_UART_RxCpltCallback(void *uart);

extern void gpsControl(void);
extern void gpsInit(void* uart);

extern void getRMCLatLongValue(double* _lat, double* _long);
extern void getGGALatLongValue(double* _lat, double* _long);

#endif /* DRIVER_GPS_GPS_H_ */
