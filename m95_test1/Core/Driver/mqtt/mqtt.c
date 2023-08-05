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

uint32_t mqtt_systick;
uint64_t mqtt_timer_cnt;

/**
 * @brief MQTT init
 * @retval None
 */
void mqttInit(void) {

	/* Start TCPIP task */
	//AT+QIREGAPP
	sendATCommand((const uint8_t*) "AT+QIREGAPP\r\n");

	/* Active the GPRS context */
	//AT+QIACT
	sendATCommand((const uint8_t*) "AT+QIACT\r\n");

//	//Configure MQTT session into SSL mode.
//	AT+QMTCFG="SSL",0,1,2
//
//	//store server root CA certificate to RAM
//	AT+QSECWRITE="RAM:cacert.pem",size,timeout
//
//	//store server root CC certificate to RAM
//	AT+QSECWRITE="RAM:clientcert.pem",size,timeout
//
//	//store server root CK certificate to RAM
//	AT+QSECWRITE="RAM:clientkey.pem",size,timeout
//
//	//Configure server root CA certificate.
//	AT+QSSLCFG="cacert",2,"RAM:cacert.pem
//
//	//Configure CC certificate.
//	AT+QSSLCFG="clientcert",2,"RAM:client.pem"
//
//	//Configure CK certificate.
//	AT+QSSLCFG="clientkey",2,"RAM:user_key.pem"
//
//	//Configure SSL parameters.
//	AT+QSSLCFG="seclevel",2,2
//
//	AT+QSSLCFG="sslversion",2,4 //SSL authentication version
//
//	AT+QSSLCFG="ciphersuite",2,"0xFFFF" //Cipher suite
//
//	AT+QSSLCFG="ignorertctime",1 //Ignore the time of authentication.
//
//	//Start MQTT SSL connection.
//	AT+QMTOPEN=0,"aws url",port
	//OK
	//+QMTOPEN: 0,0
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
//		AT+QMTPUB=0,1,1,0,"topic"
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

