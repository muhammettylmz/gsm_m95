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
#include "mqtt.h"
#include "main.h"
#include "m95.h"

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
#define SECWRITE_CC_FMT 			(const uint8_t*)"AT+QSECWRITE=\"RAM:clientcert.pem\",1220,100\r\n"
#define SECWRITE_CK_FMT 			(const uint8_t*)"AT+QSECWRITE=\"RAM:clientkey.pem\",1679,100\r\n"

#define MQTT_OPEN_FMT				(const uint8_t*)"AT+QMTOPEN=0,\"a16f5x7vu3zfui-ats.iot.eu-central-1.amazonaws.com\",8883\r\n"
#define MQTT_OPEN_SUCCESS_FMT       (uint8_t*)"+QMTOPEN: 0,0" //75 saniye beklemeli olabilir. datasheet e bak

#define MQTT_CLIENT_CONN_FMT		(const uint8_t*)"AT+QMTCONN=0,\"yehhep\""
#define MQTT_CL_CONN_SUCCESS_FMT 	(uint8_t*)"+QMTCONN: 0,0,0"

// %s yerine sprintf ile ilgili id girilmesi lazım
#define MQTT_PUB_ST_TOPIC_FMT		(const uint8_t*)"AT+QMTPUB=0,0,0,0,\"yehhep/%d/status\"\r\n"
#define MQTT_PUB_SUCCESS_FMT 		(uint8_t*)"+QMTPUB: 0,0,0"

#define PUB_MSG_JSON_FMT 			(const uint8_t*)"{\"working\":%s,\"km\":%d,\"speed\":%d,\"fuel\": %d,\"location\":{\"latitude\":%0.6f,\"longitude\":%0.6f}}"

unsigned char pubMessage[512];

uint32_t mqtt_systick;
uint64_t mqtt_timer_cnt;
uint8_t m_mqttConfigState;

uint8_t deleteCertKey(void);
uint8_t writeCertKey(void);

uint8_t getMQTTConfigState(void) {
	return m_mqttConfigState;
}

void setMQTTConfigState(mqttConfigState_e state) {
	m_mqttConfigState = state;
}

uint32_t getMqttSystick(void) {
	return mqtt_systick;
}

uint32_t mqttInitPrevTick = 0;
/**
 * @brief MQTT init
 * @retval None
 */
void mqttInit(void) {

	if (getModuleConfigState() != MODULE_CONFIG_FINISH) {
		//setModuleConfigState(MODULE_CONFIG_START);
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

//	uint8_t whileState = 1;
	static mqttConfig_e state = MQTT_CFG;

//	uint32_t prevtimeout = getMqttSystick();
	static uint8_t retry = 0;

	if(mqttInitPrevTick == 0){
		mqttInitPrevTick = getMqttSystick();
	}

//	while (whileState) {
		switch (state) {
		case MQTT_CFG: {
			if (!sendATCommand(MQTT_CFG_FMT)) {
				state++;
			}
			break;
		}
		case MQTT_CFG_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case MQTT_SSL_CERT_DELETE: {
			if (deleteCertKey()) {
				state++;
			}
			else {
//				whileState = 0;
				state = EXIT;
				m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
			}
			break;
		}
		case MQTT_SSL_CERT_WRITE: {
			if (writeCertKey()) {
				state++;
			}
			else {
//				whileState = 0;
				state = EXIT;
				m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
			}
			break;
		}
		case SSL_CFG_CA_KEY: {
			if (!sendATCommand(SSLCFG_CA_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_CA_KEY_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_CC_KEY: {
			if (!sendATCommand(SSLCFG_CC_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_CC_KEY_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_CK_KEY: {
			if (!sendATCommand(SSLCFG_CK_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_CK_KEY_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_SECLEVEL: {
			if (!sendATCommand(SSLCFG_SECLEVL_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_SECLEVEL_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_SSLVERSION: {
			if (!sendATCommand(SSLCFG_SSLVER_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_SSLVERSION_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_CIPHERSUITE: {
			if (!sendATCommand(SSLCFG_CHIPHERSUIT_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_CIPHERSUITE_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case SSL_CFG_IGNORERTCTIME: {
			if (!sendATCommand(SSLCFG_IGNRRTCTIME_FMT)) {
				state++;
			}
			break;
		}
		case SSL_CFG_IGNORERTCTIME_WAIT: {
			if (getRecvCompleted() && findATCommandResp((uint8_t*) "OK")) {
				state++;
			}
			break;
		}
		case EXIT:
			mqttInitPrevTick = 0;
			retry = 0;
			state = MQTT_CFG;
			m_mqttConfigState = MQTT_CONFIG_FINISH;
		default:
//			whileState = 0;
		break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getMqttSystick() - mqttInitPrevTick) >= 1000 && retry < 3) {
			state = MQTT_CFG;
			retry++;
		}
		else if (retry >= 3) {
//			whileState = 0;
			mqttInitPrevTick = 0;
			retry = 0;
			state = MQTT_CFG;
			m_mqttConfigState = MQTT_CONFIG_TIMEOUT;
			// config module error
		}
		else {
			HAL_Delay(5);
		}
//	}
//
//	//Start MQTT SSL connection.
//	AT+QMTOPEN=0,"aws url",port
	//OK
	//+QMTOPEN: 0,0
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
		break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getMqttSystick() - prevtimeout) >= 400 && retry < 3) {
			state = 0;
			retry++;
		}
		else if (retry >= 3) {
			whileBreak = 0;
			err = 1;
			// config module error
		}
		else {
			HAL_Delay(5);
		}
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
			}
			break;
		}
		case WRITE_CCCERT_SEND_WAIT: {
			if (getRecvCompleted() && (findATCommandResp((uint8_t*) "+QSECWRITE: 1220,2f6c"))) {
				state++;
			}
			break;
		}
		case WRITE_CKCERT: {
			if (!sendATCommand(SECWRITE_CK_FMT)) {
				state++;
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
		break;
		}

		// komutların cevabı gelmez ise kontrol mekanizması konuldu.
		if ((getMqttSystick() - prevtimeout) >= 400 && retry < 3) {
			state = 0;
			retry++;
		}
		else if (retry >= 3) {
			whileBreak = 0;
			retry = 0;
			err = 1;
			// config module error
		}
		else {
			HAL_Delay(5);
		}
	}
	return err;
}
/**
 * @brief connect MQTT broker server
 * @retval 0 is connect, others errors
 */
uint8_t connectBroker(uint8_t *client) {
	//Connect to MQTT server.
//		AT+QMTCONN=0,"client"
	// OK
	// +QMTCONN: 0,0,0
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
 * @retval 0 is success, others errors
 */
uint8_t mqttPubMessage(uint8_t *topic, uint8_t *value) {
	//Publish messages.
//		AT+QMTPUB=0,0,0,0,"topic"
	// > value ctrl+z(0x1A);
	//OK
	//+QMTPUB: 0,0,0
	return 0;
}

void MQTT_Virtual_Systick_Handler(void) {
	mqtt_systick++;
}

void MQTT_Virtual_TIM_ElapsedCallback(void *tim) {
	(void) tim;
	mqtt_timer_cnt++;
}

void mqttControl(void) {
	if(getMQTTConfigState() == MODULE_CONFIG_START){
		mqttInit();
	}
}
