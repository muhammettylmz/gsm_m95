//Header files
#include "MY_LIS3DSH.h"
#include "main.h"
#include "uart_debug.h"
#include <math.h>

//SPI Chip Select
#define _LIS3DHS_CS_ENBALE		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);
#define _LIS3DHS_CS_DISABLE		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);

//Library variables
//1. SPI handle
static SPI_HandleTypeDef accSPI_Handle;

//2. Sensitivity value
static float lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_06G;
//3. bias variables
static float __X_Bias = 0.0f;
static float __Y_Bias = 0.0f;
static float __Z_Bias = 0.0f;
//4. scaling variables
static float __X_Scale = 1.0f;
static float __Y_Scale = 1.0f;
static float __Z_Scale = 1.0f;

//Functions definitions
//Private functions
//1. Write IO
void LIS3DSH_WriteIO(uint8_t reg, uint8_t *dataW, uint8_t size) {
	uint8_t spiReg = reg;
	//Enable CS
	_LIS3DHS_CS_ENBALE
	;
	//set register value
	HAL_SPI_Transmit(&accSPI_Handle, &spiReg, 1, 10);
	//Transmit data
	HAL_SPI_Transmit(&accSPI_Handle, dataW, size, 10);
	//Disable CS
	_LIS3DHS_CS_DISABLE
	;
}
//2. Read IO
void LIS3DSH_ReadIO(uint8_t reg, uint8_t *dataR, uint8_t size) {
	uint8_t spiBuf[4];
	spiBuf[0] = reg | 0x80;
	//Enable CS
	_LIS3DHS_CS_ENBALE
	;
	//set register value
	HAL_SPI_Transmit(&accSPI_Handle, spiBuf, 1, 10);
	//Transmit data
	HAL_SPI_Receive(&accSPI_Handle, spiBuf, size, 10);
	//Disable CS
	_LIS3DHS_CS_DISABLE
	;

	for (uint8_t i = 0; i < (size & 0x3); i++) {
		dataR[i] = spiBuf[i];
	}
}

//1. Accelerometer initialise function
void LIS3DSH_Init(SPI_HandleTypeDef *accSPI, LIS3DSH_InitTypeDef *accInitDef) {
	uint8_t spiData = 0;

	memcpy(&accSPI_Handle, accSPI, sizeof(*accSPI));
	//** 1. Enable Axes and Output Data Rate **//
	//Set CTRL REG4 settings value
	spiData |= (accInitDef->enableAxes & 0x07);		//Enable Axes
	spiData |= (accInitDef->dataRate & 0xF0);			//Output Data Rate
	//Write to accelerometer
	LIS3DSH_WriteIO(LIS3DSH_CTRL_REG4_ADDR, &spiData, 1);

	//** 2. Full-Scale selection, Anti-aliasing BW, self test and 4-wire SPI **//
	spiData = 0;
	spiData |= (accInitDef->antiAliasingBW & 0xC0);		//Anti-aliasing BW
	spiData |= (accInitDef->fullScale & 0x38);				//Full-Scale
	//Write to accelerometer
	LIS3DSH_WriteIO(LIS3DSH_CTRL_REG5_ADDR, &spiData, 1);

	//** 3. Interrupt Configuration **//
	if (accInitDef->interruptEnable) {
		spiData = 0x88;
		//Write to accelerometer
		LIS3DSH_WriteIO(LIS3DSH_CTRL_REG3_ADDR, &spiData, 1);
	}

	//Assign sensor sensitivity (based on Full-Scale)
	switch (accInitDef->fullScale) {
	case LIS3DSH_FULLSCALE_2:
		lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_06G;
	break;

	case LIS3DSH_FULLSCALE_4:
		lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_12G;
	break;

	case LIS3DSH_FULLSCALE_6:
		lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_18G;
	break;

	case LIS3DSH_FULLSCALE_8:
		lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_24G;
	break;

	case LIS3DSH_FULLSCALE_16:
		lis3dsh_Sensitivity = LIS3DSH_SENSITIVITY_0_73G;
	break;
	}
	_LIS3DHS_CS_DISABLE
	;
}
//2. Get Accelerometer raw data
LIS3DSH_DataRaw LIS3DSH_GetDataRaw(void) {
	uint8_t spiBuf[2];
	LIS3DSH_DataRaw tempDataRaw;
	//Read X data
	LIS3DSH_ReadIO(LIS3DSH_OUT_X_L_ADDR, spiBuf, 2);
	tempDataRaw.x = ((spiBuf[1] << 8) + spiBuf[0]);

	//Read Y data
	LIS3DSH_ReadIO(LIS3DSH_OUT_Y_L_ADDR, spiBuf, 2);
	tempDataRaw.y = ((spiBuf[1] << 8) + spiBuf[0]);

	//Read Z data
	LIS3DSH_ReadIO(LIS3DSH_OUT_Z_L_ADDR, spiBuf, 2);
	tempDataRaw.z = ((spiBuf[1] << 8) + spiBuf[0]);

	return tempDataRaw;

}
//3. Get Accelerometer mg data
LIS3DSH_DataScaled LIS3DSH_GetDataScaled(void) {
	//Read raw data
	LIS3DSH_DataRaw tempRawData = LIS3DSH_GetDataRaw();
	;
	//Scale data and return 
	LIS3DSH_DataScaled tempScaledData;
	tempScaledData.x = (tempRawData.x * lis3dsh_Sensitivity * __X_Scale) + 0.0f - __X_Bias;
	tempScaledData.y = (tempRawData.y * lis3dsh_Sensitivity * __Y_Scale) + 0.0f - __Y_Bias;
	tempScaledData.z = (tempRawData.z * lis3dsh_Sensitivity * __Z_Scale) + 0.0f - __Z_Bias;
//	tempScaledData.x = tempRawData.x * lis3dsh_Sensitivity;
//	tempScaledData.y = tempRawData.y * lis3dsh_Sensitivity;
//	tempScaledData.z = tempRawData.z * lis3dsh_Sensitivity;
	return tempScaledData;
}
//4. Poll for Data Ready
bool LIS3DSH_PollDRDY(uint32_t msTimeout) {
	uint8_t Acc_status;
	uint32_t startTick = HAL_GetTick();
	do {
		//Read status register with a timeout
		LIS3DSH_ReadIO(LIS3DSH_STATUS_ADDR, &Acc_status, 1);
		if (Acc_status & LIS3DSH_STATUS_ADDR)
			break;

	} while ((Acc_status & LIS3DSH_STATUS_ADDR) == 0 && (HAL_GetTick() - startTick) < msTimeout);
	if (Acc_status & 0x07) {
		return true;
	}
	return false;

}

//** Calibration functions **//
//1. Set X-Axis calibrate
void LIS3DSH_X_calibrate(float x_min, float x_max) {
	__X_Bias = (x_max + x_min) / 2.0f;
	__X_Scale = (2 * 1000) / (x_max - x_min);
}
//2. Set Y-Axis calibrate
void LIS3DSH_Y_calibrate(float y_min, float y_max) {
	__Y_Bias = (y_max + y_min) / 2.0f;
	__Y_Scale = (2 * 1000) / (y_max - y_min);
}
//3. Set Z-Axis calibrate
void LIS3DSH_Z_calibrate(float z_min, float z_max) {
	__Z_Bias = (z_max + z_min) / 2.0f;
	__Z_Scale = (2 * 1000) / (z_max - z_min);
}

#define ACC_DATA_BUFF_CNT					50
#define ACC_X_AXES_SHAKE_TH					40.0f
#define ACC_Y_AXES_SHAKE_TH					40.0f
#define ACC_Z_AXES_SHAKE_TH					50.0f
#define ACC_X_AXES_SHAKE_CNT_TH 			10
#define ACC_Y_AXES_SHAKE_CNT_TH 			10
#define ACC_Z_AXES_SHAKE_CNT_TH 			10
#define ACC_CIRCULER_BUFF_SHAKE_DETECT_TH 	2

#define STATUS_REG_ZYXDA_INDIS				3
#define ACC_XYZ_NEW_DATA_AVAILABLE(reg)		(reg & (1 << STATUS_REG_ZYXDA_INDIS))

typedef struct {
	uint16_t xAxesShakeCnt;
	uint16_t yAxesShakeCnt;
	uint16_t zAxesShakeCnt;
	uint8_t xAxesShakeDetect;
	uint8_t yAxesShakeDetect;
	uint8_t zAxesShakeDetect;
} accShakeDetect_t;

LIS3DSH_DataScaled m_accData[ACC_DATA_BUFF_CNT];
volatile uint8_t m_drdyFlag = 0;
uint8_t m_accStatus;
uint8_t m_accDataCnt = 0;
LIS3DSH_InitTypeDef m_accConfigDef;
accShakeDetect_t m_accShakeDetect;
uint32_t m_memsSystick;
double xabs;
double yabs;
double zabs;

uint32_t getMEMsSystick(void) {
	return m_memsSystick;
}

accAxisShake_t getAccAllAxisShake(void){
	accAxisShake_t axisShake = {0};
	axisShake.x = m_accShakeDetect.xAxesShakeDetect;
	axisShake.y = m_accShakeDetect.yAxesShakeDetect;
	axisShake.z = m_accShakeDetect.zAxesShakeDetect;
	return axisShake;
}

void memsInit(void *spi) {
	m_accConfigDef.dataRate = LIS3DSH_DATARATE_100;
	m_accConfigDef.fullScale = LIS3DSH_FULLSCALE_16;
	m_accConfigDef.antiAliasingBW = LIS3DSH_FILTER_BW_50;
	m_accConfigDef.enableAxes = LIS3DSH_XYZ_ENABLE;
	m_accConfigDef.interruptEnable = true;
	LIS3DSH_Init((SPI_HandleTypeDef*) spi, &m_accConfigDef);

	LIS3DSH_X_calibrate(-1000.0, 980.0);
	LIS3DSH_Y_calibrate(-1020.0, 1040.0);
	LIS3DSH_Z_calibrate(-920.0, 1040.0);
}

void checkShakeAxesCnt(LIS3DSH_DataScaled *axesData, uint8_t lastIndis, accShakeDetect_t *cnt) {
	xabs = fabs(fabs(axesData[lastIndis].x) - fabs(axesData[lastIndis - 1].x));
	if (xabs > ACC_X_AXES_SHAKE_TH) {
		cnt->xAxesShakeCnt++;
	}
	yabs = fabs(fabs(axesData[lastIndis].y) - fabs(axesData[lastIndis - 1].y));
	if (yabs > ACC_Y_AXES_SHAKE_TH) {
		cnt->yAxesShakeCnt++;
	}
	zabs = fabs(fabs(axesData[lastIndis].z) - fabs(axesData[lastIndis - 1].z));
	if (zabs > ACC_Z_AXES_SHAKE_TH) {
		cnt->zAxesShakeCnt++;
	}
}

void checkAxesShakeDetect(accShakeDetect_t *detect) {
	if (detect->xAxesShakeCnt > ACC_X_AXES_SHAKE_CNT_TH) {
		detect->xAxesShakeCnt = 0;
		detect->xAxesShakeDetect++;
	}
	if (detect->yAxesShakeCnt > ACC_Y_AXES_SHAKE_CNT_TH) {
		detect->yAxesShakeCnt = 0;
		detect->yAxesShakeDetect++;
	}
	if (detect->zAxesShakeCnt > ACC_Z_AXES_SHAKE_CNT_TH) {
		detect->zAxesShakeCnt = 0;
		detect->zAxesShakeDetect++;
	}
}
void clearAxesShakeCnt(accShakeDetect_t *axes) {
	axes->xAxesShakeCnt = 0;
	axes->yAxesShakeCnt = 0;
	axes->zAxesShakeCnt = 0;
}

void memsControl(void) {

//	if(getMEMsSystick() % 11 != 1){
//		return;
//	}
	static uint8_t circulerBuffCnt = 0;
	LIS3DSH_ReadIO(LIS3DSH_STATUS_ADDR, &m_accStatus, 1);
	if (ACC_XYZ_NEW_DATA_AVAILABLE(m_accStatus)) {

		m_accData[m_accDataCnt] = LIS3DSH_GetDataScaled();
//		memsDebugAcc("%.6f\t%.6f\t%.6f\r\n", m_accData[m_accDataCnt].x,
//				m_accData[m_accDataCnt].y, m_accData[m_accDataCnt].z);
		if (m_accDataCnt > 1) {
			checkShakeAxesCnt(m_accData, m_accDataCnt, &m_accShakeDetect);
		}

		m_accDataCnt++;
		if (m_accDataCnt >= ACC_DATA_BUFF_CNT) {
			m_accDataCnt = 0;
			circulerBuffCnt++;
			if (circulerBuffCnt > ACC_CIRCULER_BUFF_SHAKE_DETECT_TH) {
				circulerBuffCnt = 0;
				checkAxesShakeDetect(&m_accShakeDetect);
				clearAxesShakeCnt(&m_accShakeDetect);
			}
		}
	}

}

void MEMS_Virtual_GPIO_EXTI(void) {
//m_drdyFlag = 1;
}

void MEMS_Virtual_Systick_Handler(void) {
	m_memsSystick++;
}

