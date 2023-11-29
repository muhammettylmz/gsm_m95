/*
 * mqtt.c
 *
 *  Created on: Jul 31, 2023
 *      Author: myilmaz
 *
 *      MQTT Quality of Service(QoS)
 *      En Fazla Bir Kez (QoS0) seviyesi, onaylanmamış veriler için kullanılır.
 *      MQTT Broker, yayıncının (Publisher) yolladığı veriyi alır ve abone (subscriber) bir kere yollar.
 *      Gönderilen verinin aboneye ulaşıp ulaşmadığı kontrol etmez. Ağda minimum bant genişliği kullanmak için kullanılır.
 *
 *      En Az Bir Kez (QoS1) seviyesi, yayıncı (publisher) tarafından gönderilen verinin aboneye (subscriber)
 *      ulaşıp ulaşmadığı kontrol eden seviyedir. Verinin karşı tarafa ulaşıp ulaşmadığını kontrol etmek
 *      için belli zaman aralıklarla aynı veriyi tekrar yollar.
 *      Abone tarafında birden fazla aynı veri olabilir.
 *
 *      Tam Olarak Bir Kez (QoS2) seviyesi, ağdaki bant genişliğini maksimum şekilde kullanan seviyedir.
 *      Yayıncı tarafından gönderilen verinin aboneye ulaşıp ulaşmadığını kontrol eder ve gönderilen
 *      veriye bir kimlik atayarak gönderir. Bu kimlik sayesinde verinin birde fazla kopyalanıp
 *      kopyalanmadığı öğrenmiş olur.
 */
#include <stdio.h>
#include <string.h>
#include "mqtt.h"
#include "main.h"
#include "m95.h"
#include "gps.h"
#include "uart_debug.h"
#include "obd2.h"
#include "MY_LIS3DSH.h"

#define GSM_TCP_IP_STACK_START_FMT 	(const uint8_t*)"AT+QIREGAPP\r\n"
#define GPRS_ACTIVE_CTX_FMT			(const uint8_t*)"AT+QIACT\r\n"

#define MQTT_CFG_FMT 				(const uint8_t*)"AT+QMTCFG=\"SSL\",0,1,2\r\n"
#define SSLCFG_CA_FMT 				(const uint8_t*)"AT+QSSLCFG=\"cacert\",2,\"RAM:cacert.pem\"\r\n"
#define SSLCFG_CC_FMT 				(const uint8_t*)"AT+QSSLCFG=\"clientcert\",2,\"RAM:clientcert.pem\"\r\n"
#define SSLCFG_CK_FMT 				(const uint8_t*)"AT+QSSLCFG=\"clientkey\",2,\"RAM:clientkey.pem\"\r\n"
#define SSLCFG_SECLEVL_FMT 			(const uint8_t*)"AT+QSSLCFG=\"seclevel\",2,2\r\n"
#define SSLCFG_SSLVER_FMT 			(const uint8_t*)"AT+QSSLCFG=\"sslversion\",2,4\r\n"
#define SSLCFG_CHIPHERSUIT_FMT  	(const uint8_t*)"AT+QSSLCFG=\"ciphersuite\",2,\"0xFFFF\"\r\n"
#define SSLCFG_IGNRRTCTIME_FMT  	(const uint8_t*)"AT+QSSLCFG=\"ignorertctime\",1\r\n"

#define SECDEL_CA_FMT 				(const uint8_t*)"AT+QSECDEL=\"RAM:cacert.pem\"\r\n"
#define SECDEL_CC_FMT 				(const uint8_t*)"AT+QSECDEL=\"RAM:clientcert.pem\"\r\n"
#define SECDEL_CK_FMT 				(const uint8_t*)"AT+QSECDEL=\"RAM:clientkey.pem\"\r\n"

#define SECWRITE_CA_FMT 			(const uint8_t*)"AT+QSECWRITE=\"RAM:cacert.pem\",1188,100\r\n"
#define SECWRITE_CC_FMT 			(const uint8_t*)"AT+QSECWRITE=\"RAM:clientcert.pem\",1219,100\r\n"
#define SECWRITE_CK_FMT 			(const uint8_t*)"AT+QSECWRITE=\"RAM:clientkey.pem\",1679,100\r\n"

#define MQTT_OPEN_FMT				(const char*)"AT+QMTOPEN=0,\"%s\",%d\r\n"
//#define MQTT_OPEN_FMT				(const uint8_t*)"AT+QMTOPEN=0,\"a16f5x7vu3zfui-ats.iot.eu-central-1.amazonaws.com\",8883\r\n"
#define MQTT_OPEN_SUCCESS_FMT       (uint8_t*)"+QMTOPEN: 0,0" //75 saniye beklemeli olabilir. datasheet e bak
#define MQTT_OPEN_REPONSE_TIME		(75200) // unit ms

#define MQTT_CLIENT_CONN_FMT		(const char*)"AT+QMTCONN=0,\"%s\"\r\n"
#define MQTT_CL_CONN_SUCCESS_FMT 	(uint8_t*)"+QMTCONN: 0,0,0"
#define MQTT_CONN_REPONSE_TIME		(20200) // unit ms

// %s yerine sprintf ile ilgili id girilmesi lazım
#define MQTT_PUB_ST_TOPIC_FMT		(const char*)"AT+QMTPUB=0,0,0,0,\"%s\"\r\n"
#define MQTT_PUB_SUCCESS_FMT 		(uint8_t*)"+QMTPUB: 0,0,0"
#define MQTT_PUB_SEND_CTRL_Z		0x1A // ascii table ctrl+z decimal 26,
#define MQTT_PUB_REQ_RESPONSE_TIME  (20200) // unit ms

#define PUB_MSG_JSON_FMT 			(const char*)"{\"working\":%s,\"km\":%lu,\"speed\":%d,\"fuel\": %d,\"location\":{\"latitude\":%.8f,\"longitude\":%.8f}}"

#define MQTT_PUBLISH_DEFAULT_TIMEOUT_MS 					10000
#define OFFSET_DIV											4
#define VEHICLE_SPEED_MAX_VALUE 							255
#define MQTT_PUBLISH_VEHICLE_SPEED_TIMEOUT_OFFSET 			((MQTT_PUBLISH_DEFAULT_TIMEOUT_MS / OFFSET_DIV) / (VEHICLE_SPEED_MAX_VALUE - 1))
#define MQTT_PUBLISH_VEHICLE_SPEED_TIMEOUT_CALC(x)			((VEHICLE_SPEED_MAX_VALUE - x) * MQTT_PUBLISH_VEHICLE_SPEED_TIMEOUT_OFFSET)

uint32_t m_vehicleSpeedTimeout = MQTT_PUBLISH_DEFAULT_TIMEOUT_MS;

unsigned char pubMessage[512];

uint32_t mqtt_systick;
uint64_t mqtt_timer_cnt;
mqttConfigState_e m_mqttConfigState;
mqttConnectState_e m_mqttConnectState = MQTT_NOT_CONNECT;
mqttOpenState_e m_mqttOpenState = MQTT_NOT_OPEN;
mqttPubReqState_e m_mqttPubReqState = MQTT_PUBLISH_IDLE;
mqttPublishReady_e m_publishReady = PUBLISH_FINISH;

uint32_t mqttConnectPrevtimeout = 0;
uint32_t mqttOpenPrevtimeout = 0;
uint32_t mqttPubReqPrevtimeout = 0;
uint32_t mqttInitPrevTick = 0;
obd2VehicleData_t vehicleData = { 0 };
uint32_t m_mqttPublishTimeoutCnt = 0;
uint8_t m_mqttPublishTimeout = 0;

uint8_t deleteCertKey(void);
uint8_t writeCertKey(void);

void setMQTTPublishReadyState(mqttPublishReady_e state) {
	m_publishReady = state;
}

mqttPublishReady_e getMQTTPublishReadyState(void) {
	return m_publishReady;
}

void setMQTTOpenState(mqttOpenState_e state) {
	m_mqttOpenState = state;
}

mqttOpenState_e getMQTTOpenState(void) {
	return m_mqttOpenState;
}

void setMQTTConnectState(mqttConnectState_e state) {
	m_mqttConnectState = state;
}

mqttConnectState_e getMQTTConnectState(void) {
	return m_mqttConnectState;
}

mqttConfigState_e getMQTTConfigState(void) {
	return m_mqttConfigState;
}

void setMQTTConfigState(mqttConfigState_e state) {
	m_mqttConfigState = state;
}

uint32_t getMqttSystick(void) {
	return mqtt_systick;
}

/**
 * @brief MQTT init
 * @retval None
 */
void mqttInit(void) {

	if (getModuleConfigState() != MODULE_CONFIG_FINISH) {
		setModuleConfigState(MODULE_CONFIG_START);
		return;
	}

	typedef enum {
		MQTT_CFG,
		MQTT_CFG_WAIT,
		MQTT_SSL_CERT_DELETE,
		MQTT_SSL_CERT_WRITE,
		SSL_CFG_CA_KEY,
		SSL_CFG_CA_KEY_WAIT,
		SSL_CFG_CC_KEY,
		SSL_CFG_CC_KEY_WAIT,
		SSL_CFG_CK_KEY,
		SSL_CFG_CK_KEY_WAIT,
		SSL_CFG_SECLEVEL,
		SSL_CFG_SECLEVEL_WAIT,
		SSL_CFG_SSLVERSION,
		SSL_CFG_SSLVERSION_WAIT,
		SSL_CFG_CIPHERSUITE,
		SSL_CFG_CIPHERSUITE_WAIT,
		SSL_CFG_IGNORERTCTIME,
		SSL_CFG_IGNORERTCTIME_WAIT,
		EXIT
	} mqttConfig_e;

	m_mqttConfigState = MQTT_CONFIG_START;

	static mqttConfig_e state = MQTT_CFG;
	static uint8_t retry = 0;

	if (mqttInitPrevTick == 0) {
		mqttInitPrevTick = getMqttSystick();
	}

	switch (state) {
	case MQTT_CFG: {
		customDebugMsg("MQTT_CFG \r\n");
		if (!sendATCommand(MQTT_CFG_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case MQTT_CFG_WAIT: {
		customDebugMsg("MQTT_CFG_WAIT \r\n");
		if (getRecvCompleted()) {
			if (findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
		}
		break;
	}
	case MQTT_SSL_CERT_DELETE: {
		customDebugMsg("MQTT_SSL_CERT_DELETE \r\n");
		if (!deleteCertKey()) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		else {
			customDebugMsg("MQTT_SSL_CERT_DELETE EXIT \r\n");
			state = EXIT;
			m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
		}
		break;
	}
	case MQTT_SSL_CERT_WRITE: {
		customDebugMsg("MQTT_SSL_CERT_WRITE \r\n");
		if (!writeCertKey()) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		else {
			customDebugMsg("MQTT_SSL_CERT_WRITE EXIT\r\n");
			state = EXIT;
			m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
		}
		break;
	}
	case SSL_CFG_CA_KEY: {
		customDebugMsg("SSL_CFG_CA_KEY \r\n");
		if (!sendATCommand(SSLCFG_CA_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_CA_KEY_WAIT: {
		customDebugMsg("SSL_CFG_CA_KEY_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_CC_KEY: {
		customDebugMsg("SSL_CFG_CC_KEY \r\n");
		if (!sendATCommand(SSLCFG_CC_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_CC_KEY_WAIT: {
		customDebugMsg("SSL_CFG_CC_KEY_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_CK_KEY: {
		customDebugMsg("SSL_CFG_CK_KEY \r\n");
		if (!sendATCommand(SSLCFG_CK_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_CK_KEY_WAIT: {
		customDebugMsg("SSL_CFG_CK_KEY_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_SECLEVEL: {
		customDebugMsg("SSL_CFG_SECLEVEL \r\n");
		if (!sendATCommand(SSLCFG_SECLEVL_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_SECLEVEL_WAIT: {
		customDebugMsg("SSL_CFG_SECLEVEL_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_SSLVERSION: {
		customDebugMsg("SSL_CFG_SSLVERSION \r\n");
		if (!sendATCommand(SSLCFG_SSLVER_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_SSLVERSION_WAIT: {
		customDebugMsg("SSL_CFG_SSLVERSION_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_CIPHERSUITE: {
		customDebugMsg("SSL_CFG_CIPHERSUITE \r\n");
		if (!sendATCommand(SSLCFG_CHIPHERSUIT_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_CIPHERSUITE_WAIT: {
		customDebugMsg("SSL_CFG_CIPHERSUITE_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case SSL_CFG_IGNORERTCTIME: {
		customDebugMsg("SSL_CFG_IGNORERTCTIME \r\n");
		if (!sendATCommand(SSLCFG_IGNRRTCTIME_FMT)) {
			state++;
			mqttInitPrevTick = getMqttSystick();
		}
		break;
	}
	case SSL_CFG_IGNORERTCTIME_WAIT: {
		customDebugMsg("SSL_CFG_IGNORERTCTIME_WAIT \r\n");
		if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
			state++;
		}
		break;
	}
	case EXIT:

	default:
		mqttInitPrevTick = 0;
		retry = 0;
		state = MQTT_CFG;
		m_mqttConfigState = MQTT_CONFIG_FINISH;
		customDebugMsg("MQTT Init SUCCESS... \r\n");
	break;
	}

	// komutların cevabı gelmez ise kontrol mekanizması konuldu.
	if ((getMqttSystick() - mqttInitPrevTick) >= 500 && retry < 3) {
		state = MQTT_CFG;
		retry++;
		mqttInitPrevTick = getMqttSystick();
	}
	else if (retry >= 3) {
		mqttInitPrevTick = 0;
		retry = 0;
		state = MQTT_CFG;
		m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
		customDebugMsg("MQTT Init TIMEOUT... \r\n");
		// config module error
	}
//	else {
//		HAL_Delay(2);
//	}
}

/**
 * @brief Delete cert and key in RAM
 * @retval 0 is success, 1 is others
 */
uint8_t deleteCertKey(void) {
	typedef enum {
		DEL_CACERT, DEL_CACERT_WAIT, DEL_CCCERT, DEL_CCCERT_WAIT, DEL_CKCERT, DEL_CKCERT_WAIT
	} deleteCert_e;

	uint8_t err = 1;
	uint8_t whileBreak = 1;
	deleteCert_e state = DEL_CACERT;
	uint32_t prevtimeout = getMqttSystick();
	uint8_t retry = 0;

	while (whileBreak) {
		switch (state) {
		case DEL_CACERT: {
			if (!sendATCommand(SECDEL_CA_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case DEL_CACERT_WAIT: {
			if (getRecvCompleted()
					&& (findATCommandResp((uint8_t*) "OK") || findATCommandResp((uint8_t*) "ERROR"))) {
				state++;
			}
			break;
		}
		case DEL_CCCERT: {
			if (!sendATCommand(SECDEL_CC_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case DEL_CCCERT_WAIT: {
			if (getRecvCompleted()
					&& (findATCommandResp((uint8_t*) "OK") || findATCommandResp((uint8_t*) "ERROR"))) {
				state++;
			}
			break;
		}
		case DEL_CKCERT: {
			if (!sendATCommand(SECDEL_CK_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case DEL_CKCERT_WAIT: {
			if (getRecvCompleted()
					&& (findATCommandResp((uint8_t*) "OK") || findATCommandResp((uint8_t*) "ERROR"))) {
				state++;
			}
			break;
		}
		default:
			whileBreak = 0;
			err = 0;
			customDebugMsg("Delete SSL Cert and Key SUCCES... \r\n");
		break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getMqttSystick() - prevtimeout) >= 400 && retry < 3) {
			state = 0;
			retry++;
			prevtimeout = getMqttSystick();
		}
		else if (retry >= 3) {
			whileBreak = 0;
			err = 1;
			// config module error
			customDebugMsg("Delete SSL Cert and Key TIMEOUT... \r\n");
		}
//		else {
//			HAL_Delay(5);
//		}
	}
	return err;
}

/**
 * @brief write cert and key in RAM
 * @retval 0 is success, 1 is others
 */
uint8_t writeCertKey(void) {
	typedef enum {
		WRITE_CACERT,  //at+qsecwrite ram
		WRITE_CACERT_WAIT,  //connect
		WRITE_CACERT_SEND,	//send cert file with uart
		WRITE_CACERT_SEND_WAIT,  //+qsecwrite ..
		WRITE_CCCERT,
		WRITE_CCCERT_WAIT,
		WRITE_CCCERT_SEND,
		WRITE_CCCERT_SEND_WAIT,
		WRITE_CKCERT,
		WRITE_CKCERT_WAIT,
		WRITE_CKCERT_SEND,
		WRITE_CKCERT_SEND_WAIT,
		EXIT
	} writeCert_e;

	uint8_t err = 1;
	uint8_t whileBreak = 1;
	writeCert_e state = WRITE_CACERT;
	uint32_t prevtimeout = getMqttSystick();
	uint8_t retry = 0;

	while (whileBreak) {
		switch (state) {
		case WRITE_CACERT: {
			if (!sendATCommand(SECWRITE_CA_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CACERT_WAIT: {
			if (getRecvCompleted()) {
				if (findATCommandResp((uint8_t*) "CONNECT")) {
					state++;
				}
				else if (findATCommandResp((uint8_t*) "Already exits")
						|| findATCommandResp((uint8_t*) "+CME ERROR")) {
					state = WRITE_CCCERT;
				}
			}
			break;
		}
		case WRITE_CACERT_SEND: {
			if (!sendUartData(awsRootCA1, sizeof(awsRootCA1))) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CACERT_SEND_WAIT: {
			if (getRecvCompleted() && (findATCommandResp((uint8_t*) "+QSECWRITE: 1188,2d13"))) {
				state++;
			}
			break;
		}
		case WRITE_CCCERT: {
			if (!sendATCommand(SECWRITE_CC_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CCCERT_WAIT: {
			if (getRecvCompleted()) {
				if (findATCommandResp((uint8_t*) "CONNECT")) {
					state++;
				}
				else if (findATCommandResp((uint8_t*) "Already exits")
						|| findATCommandResp((uint8_t*) "+CME ERROR")) {
					state = WRITE_CACERT;
				}
			}
			break;
		}
		case WRITE_CCCERT_SEND: {
			if (!sendUartData(clientCert, sizeof(clientCert))) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CCCERT_SEND_WAIT: {
			if (getRecvCompleted() && (findATCommandResp((uint8_t*) "+QSECWRITE: 1219,2f6c"))) {
				state++;
			}
			break;
		}
		case WRITE_CKCERT: {
			if (!sendATCommand(SECWRITE_CK_FMT)) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CKCERT_WAIT: {
			if (getRecvCompleted()) {
				if (findATCommandResp((uint8_t*) "CONNECT")) {
					state++;
				}
				else if (findATCommandResp((uint8_t*) "Already exits")
						|| findATCommandResp((uint8_t*) "+CME ERROR")) {
					state = EXIT;  // exit while
				}
			}
			break;
		}
		case WRITE_CKCERT_SEND: {
			if (!sendUartData(clientPrivateKey, sizeof(clientPrivateKey))) {
				state++;
				prevtimeout = getMqttSystick();
			}
			break;
		}
		case WRITE_CKCERT_SEND_WAIT: {
			if (getRecvCompleted() && (findATCommandResp((uint8_t*) "+QSECWRITE: 1679,13b"))) {
				state++;
			}
			break;
		}
			//fallth
		case EXIT:
		default:
			whileBreak = 0;
			err = 0;
			customDebugMsg("Write SSL Cert and Key SUCCESS... \r\n");
		break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getMqttSystick() - prevtimeout) >= 400 && retry < 3) {
			state = 0;
			retry++;
			prevtimeout = getMqttSystick();
		}
		else if (retry >= 3) {
			whileBreak = 0;
			retry = 0;
			err = 1;
			// config module error
			customDebugMsg("Write SSL Cert and Key TIMEOUT... \r\n");
		}
//		else {
//			HAL_Delay(5);
//		}
	}
	return err;
}
/**
 * @brief connect MQTT client
 * @retval 0 is connect, others errors
 */
uint8_t mqttConnect(uint8_t *client) {
	typedef enum {
		MQTT_CONNECT, MQTT_CONNECT_OK_WAIT, MQTT_CONNECT_SUCCES_WAIT, EXIT
	} mqttConenct_e;

	static mqttConenct_e state = MQTT_CONNECT;
	m_mqttConnectState = MQTT_NOT_CONNECT;

	if (mqttConnectPrevtimeout == 0) {
		mqttConnectPrevtimeout = getMqttSystick();
	}

	uint8_t connectMsg[64] = { 0 };

	switch (state) {
	case MQTT_CONNECT: {
		customDebugMsg("MQTT_CONNECT: client: %s\r\n", client);
		sprintf((char*) connectMsg, MQTT_CLIENT_CONN_FMT, client);
		if (!sendATCommand(connectMsg)) {
			state++;
		}
		break;
	}
	case MQTT_CONNECT_OK_WAIT: {
		customDebugMsg("MQTT_CONNECT_OK_WAIT\r\n", client);
		if (getRecvCompleted()) {
			if ((findATCommandResp((uint8_t*) "OK"))) {
				state++;
				mqttConnectPrevtimeout = getMqttSystick();
			}
			else {
				state = MQTT_CONNECT;
				mqttConnectPrevtimeout = 0;
				m_mqttConnectState = MQTT_NOT_CONNECT;
			}
		}
		break;
	}
	case MQTT_CONNECT_SUCCES_WAIT: {
		if (getRecvCompleted()) {
			if ((findATCommandResp(MQTT_CL_CONN_SUCCESS_FMT))) {
				state = EXIT;
				m_mqttConnectState = MQTT_CONNECTED;
				customDebugMsg("MQTT Client Connect SUCCESS... \r\n");
			}
			else {
				m_mqttConnectState = MQTT_NOT_CONNECT;

			}
		}
		break;
	}
	case EXIT:
	default:
		state = MQTT_CONNECT;
		mqttConnectPrevtimeout = 0;
		m_mqttPublishTimeoutCnt = getMqttSystick();
	break;
	}

	// AT komut maksimum response time
	if ((getMqttSystick() - mqttConnectPrevtimeout) >= MQTT_CONN_REPONSE_TIME) {
		mqttConnectPrevtimeout = 0;
		state = MQTT_CONNECT;
		m_mqttConnectState = MQTT_CONNECT_TIMEOUT;
		customDebugMsg("MQTT Client Connect TIMEOUT... \r\n");
	}

	return 0;
}

/**
 * @brief Open MQTT broker
 * @retval 0 is connect, others errors
 */
uint8_t mqttOpenBroker(uint8_t *endpoint, uint16_t port) {
	typedef enum {
		MQTT_OPEN, MQTT_OPEN_OK_WAIT, MQTT_OPEN_SUCCES_WAIT, EXIT
	} mqttOpen_e;

	static mqttOpen_e state = MQTT_OPEN;
	m_mqttConnectState = MQTT_NOT_OPEN;

	if (mqttOpenPrevtimeout == 0) {
		mqttOpenPrevtimeout = getMqttSystick();
	}
	uint8_t openMsg[64] = { 0 };
	switch (state) {
	case MQTT_OPEN: {
		customDebugMsg("MQTT_OPEN: url:%s, port:%d \r\n", endpoint, port);
		sprintf((char*) openMsg, MQTT_OPEN_FMT, endpoint, port);
		customDebugMsg("MQTT_OPEN: sprintf: %s \r\n", openMsg);
		if (!sendATCommand(openMsg)) {
			state++;
		}
		break;
	}
	case MQTT_OPEN_OK_WAIT: {
		customDebugMsg("MQTT_OPEN_OK_WAIT\r\n");
		if (getRecvCompleted()) {
			if ((findATCommandResp((uint8_t*) "OK"))) {
				state++;
				mqttOpenPrevtimeout = getMqttSystick();
			}
			else {
				state = MQTT_OPEN;
				mqttOpenPrevtimeout = 0;
				m_mqttOpenState = MQTT_NOT_OPEN;
			}
		}
		break;
	}
	case MQTT_OPEN_SUCCES_WAIT: {
		//customDebugMsg("MQTT_OPEN_SUCCES_WAIT\r\n");
		if (getRecvCompleted()) {
			if ((findATCommandResp(MQTT_OPEN_SUCCESS_FMT))) {
				state = EXIT;
				m_mqttOpenState = MQTT_OPEN_SUCCESS;
				customDebugMsg("MQTT Broker open SUCCESS... \r\n");
			}
			else {
				m_mqttOpenState = MQTT_NOT_OPEN;

			}
		}
		break;
	}
	case EXIT:
	default:
		state = MQTT_OPEN;
		mqttOpenPrevtimeout = 0;
	break;
	}

	// AT komut maksimum response time
	if ((getMqttSystick() - mqttOpenPrevtimeout) >= MQTT_OPEN_REPONSE_TIME) {
		mqttOpenPrevtimeout = 0;
		state = MQTT_OPEN;
		m_mqttOpenState = MQTT_OPEN_TIMEOUT;
		customDebugMsg("MQTT Broker open TIMEOUT... \r\n");
	}

	return 0;
}

/**
 * @brief disconnect MQTT broker server
 * @retval 0 is disconnect, others errors
 */
uint8_t disconnectBroker(void) {
	//Disconnect a client from MQTT server.
//		AT+QMTDISC=0
	return 0;
}

/**
 * @brief send message MQTT topic,value is json format
 * value değeri json formatında hazır şekilde gelicek.
 * topic değeri ise ilgili topic ve id si içerinsinde olacak.
 * @retval 0 is success, others errors
 */
uint8_t mqttPubMessage(uint8_t *topic, uint8_t *value, uint16_t len) {
	typedef enum {
		MQTT_PUB_REQ, MQTT_PUB_REQ_WAIT,  // recv > value
		MQTT_PUB_SEND,		//json data send
		MQTT_PUB_SEND_OK_WAIT,  // OK response
		MQTT_PUB_SEND_SUCCESS_WAIT,  //recv +QMTPUB: 0,0,0
		EXIT
	} mqttPubReq_e;

	static mqttPubReq_e state = MQTT_PUB_REQ;
	m_mqttPubReqState = MQTT_PUBLISH_IDLE;

	if (mqttPubReqPrevtimeout == 0) {
		mqttPubReqPrevtimeout = getMqttSystick();
	}
	uint8_t pubMsg[128] = { 0 };

	switch (state) {
	case MQTT_PUB_REQ: {
		customDebugMsg("MQTT_PUB_REQ: topic: %s \r\n", topic);
		sprintf((char*) pubMsg, MQTT_PUB_ST_TOPIC_FMT, topic);
		if (!sendATCommand(pubMsg)) {
			state++;
		}
		break;
	}
	case MQTT_PUB_REQ_WAIT: {
		customDebugMsg("MQTT_PUB_REQ_WAIT \r\n");
		if (getRecvCompleted()) {
			if ((findATCommandResp((uint8_t*) ">"))) {

				state++;
			}
			else {
				state = MQTT_PUB_REQ;
				mqttPubReqPrevtimeout = getMqttSystick();
				m_mqttPubReqState = MQTT_PUBLISH_IDLE;
			}
		}
		break;
	}
	case MQTT_PUB_SEND: {

		//The maximum length of the data is 1548 bytes and the data beyond 1548 bytes will be omitted.
		value[len] = MQTT_PUB_SEND_CTRL_Z;  // After inputting data, tap Ctrl+Z to send.
		value[len + 1] = '\0';  // for strlen;
		customDebugMsg("MQTT_PUB_SEND: json:%s\r\n", value);
		if (!sendUartData(value, len + 1)) {
			state++;
			mqttPubReqPrevtimeout = getMqttSystick();
			customDebugMsg("MQTT Publish send Data over UART is SUCCESS...\r\n");
		}
		else {
			state = MQTT_PUB_REQ;
			mqttPubReqPrevtimeout = getMqttSystick();
			m_mqttPubReqState = MQTT_PUBLISH_IDLE;
		}
		break;
	}
	case MQTT_PUB_SEND_OK_WAIT: {
		customDebugMsg("MQTT_PUB_SEND_OK_WAIT \r\n");
		if (getRecvCompleted()) {
			if ((findATCommandResp((uint8_t*) "OK"))) {

				state++;

				mqttPubReqPrevtimeout = getMqttSystick();
				customDebugMsg("MQTT Publish send OK response ... \r\n");
				if ((findATCommandResp(MQTT_PUB_SUCCESS_FMT))) {
					state = EXIT;
					m_mqttPubReqState = MQTT_PUBLISH_SUCCESS;
					setMQTTPublishReadyState(PUBLISH_FINISH);
					customDebugMsg("MQTT Publish send SUCCESS... \r\n");
				}
			}
			else {
				state = MQTT_PUB_REQ;
				mqttPubReqPrevtimeout = 0;
				m_mqttPubReqState = MQTT_PUBLISH_IDLE;
			}
		}
		break;
	}
	case MQTT_PUB_SEND_SUCCESS_WAIT: {
		if (getRecvCompleted()) {
			if ((findATCommandResp(MQTT_PUB_SUCCESS_FMT))) {
				state = EXIT;
				m_mqttPubReqState = MQTT_PUBLISH_SUCCESS;
				setMQTTPublishReadyState(PUBLISH_FINISH);
				customDebugMsg("MQTT Publish send SUCCESS... \r\n");
			}
			else {
				m_mqttPubReqState = MQTT_PUBLISH_IDLE;
				customDebugMsg("MQTT Publish send Error... \r\n");
			}
		}
		break;
	}
	case EXIT:
	default:
		state = MQTT_PUB_REQ;
		mqttPubReqPrevtimeout = 0;
	break;
	}

	// AT komut maksimum response time
	if ((getMqttSystick() - mqttPubReqPrevtimeout) >= MQTT_PUB_REQ_RESPONSE_TIME) {
		mqttPubReqPrevtimeout = 0;
		state = MQTT_PUB_REQ;
		m_mqttPubReqState = MQTT_PUBLISH_TIMEOUT;
		customDebugMsg("MQTT Publish send TIMEOUT... \r\n");
	}
	//HAL_Delay(1000);
	return 0;
}

void MQTT_Virtual_Systick_Handler(void) {
	mqtt_systick++;

	if (m_mqttPublishTimeoutCnt != 0) {
		if ((mqtt_systick - m_mqttPublishTimeoutCnt) >= m_vehicleSpeedTimeout) {
			m_mqttPublishTimeout = 1;
			m_mqttPublishTimeoutCnt = mqtt_systick;
		}
	}
}

void MQTT_Virtual_TIM_ElapsedCallback(void *tim) {
	(void) tim;
	mqtt_timer_cnt++;
}

uint8_t test_topic[64];
uint16_t test_id = 666;
uint8_t test_json_value[512];
uint8_t test_working[5] = "true\0";
uint16_t test_km = 10;
uint16_t test_speed = 0;
uint16_t test_fuel = 5000;
double test_longitude = 42.12;
double test_latitude = 29.12;
uint32_t test_prevtimeout = 0;

uint8_t rawData[128];

void mqttControl(void) {
	if (getMQTTConfigState() != MQTT_CONFIG_FINISH) {
		mqttInit();
		return;
	}
	//check mqtt open closed. +QMTSTAT: 0,1
	if (getRecvCompleted()) {
		if (findATCommandResp((uint8_t*) "+QMTSTAT:")) {
			getRxGSMRawData(rawData);
			// Quectel_GSM_MQTT_Application_Note_V1.3.pdf
			// 4.1 +QMTSTAT Indicate State Change in MQTT Link Layer
			if (rawData[12] != '0') {
				setMQTTOpenState(MQTT_NOT_OPEN);
			}
		}
	}

	if (getMQTTOpenState() != MQTT_OPEN_SUCCESS) {
		mqttOpenBroker(MQTT_AWS_URL, MQTT_AWS_PORT);
		return;
	}
//
	if (getMQTTConnectState() != MQTT_CONNECTED) {
		mqttConnect(MQTT_CLIENT);
		return;
	}

	if (vehicleData.vehicleSpeed != 0) {
		m_vehicleSpeedTimeout = MQTT_PUBLISH_VEHICLE_SPEED_TIMEOUT_CALC(vehicleData.vehicleSpeed);
	}
	else {
		m_vehicleSpeedTimeout = MQTT_PUBLISH_DEFAULT_TIMEOUT_MS;
	}

	if ((getMQTTConnectState() == MQTT_CONNECTED && m_publishReady == PUBLISH_READY
			&& getObd2PeroidicDataCompletedState() == OBD2_PERIODIC_DATA_COMPLETED)
			|| m_mqttPublishTimeout) {

		m_mqttPublishTimeout = 0;
		m_mqttPublishTimeoutCnt = getMqttSystick();

		getGGALatLongValue(&test_latitude, &test_longitude);
		vehicleData = getPeriodicObdVehicleData();
		setObd2PeroidicDataCompletedState(OBD2_PERIODIC_DATA_IDLE);
//		accAxisShake_t accAllAxisShake = getAccAllAxisShake();

		sprintf((char*) test_topic, MQTT_AWS_TOPIC, test_id);
		sprintf((char*) test_json_value, PUB_MSG_JSON_FMT, test_working,
				vehicleData.vehicleOdometer, vehicleData.vehicleSpeed, vehicleData.fuelLevelInput,
				test_latitude, test_longitude);

		mqttPubMessage(test_topic, test_json_value, (strlen((char*) test_json_value) + 2));
	}

	/*
	 * */
}
