#include <Arduino.h>
extern "C" {
#include <user_interface.h>
};
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>

// platformio lib install 66
#include <HttpClient.h>

#include "utils.h"

#include "gf.hpp"
#include "mcp3425.hpp"

#include "config.h"

WiFiUDP UDP;
WiFiClient wifiClient;
HttpClient http(wifiClient);
GrowthForecastClient gf(http, GF_HOST, GF_USER, GF_PASS);

struct {
	char SSID[255] = WIFI_SSID;
	char Password[255] = WIFI_PASS;
}  wifi_config;

bool startWifi(int timeout) {
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	Serial.println("Reading wifi_config");
	Serial.println("wifi_config:");
	Serial.print("SSID: ");
	Serial.print(wifi_config.SSID);
	Serial.print("\n");
	if (strlen(wifi_config.SSID) == 0) {
		Serial.println("SSID is not configured");
		return false;
	}

	WiFi.begin(wifi_config.SSID, wifi_config.Password);
	int time = 0;
	for (;;) {
		switch (WiFi.status()) {
			case WL_CONNECTED:
				Serial.println("connected!");
				WiFi.printDiag(Serial);
				Serial.print("IPAddress: ");
				Serial.println(WiFi.localIP());
				return true;
			case WL_CONNECT_FAILED:
				Serial.println("connect failed");
				return false;
		}
		delay(250);
		Serial.print(".");
		time++;
		if (time >= timeout) {
			break;
		}
	}
	return false;
}

void setup_ota () {
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Hostname defaults to esp8266-[ChipID]
	// ArduinoOTA.setHostname("myesp8266");

	// No authentication by default
	// ArduinoOTA.setPassword((const char *)"123");

	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("End");
		ESP.restart();
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		switch (error) {
			case OTA_AUTH_ERROR: Serial.println("Auth Failed"); break;
			case OTA_BEGIN_ERROR: Serial.println("Begin Failed"); break;
			case OTA_CONNECT_ERROR: Serial.println("Connect Failed"); break;
			case OTA_RECEIVE_ERROR: Serial.println("Receive Failed"); break;
			case OTA_END_ERROR: Serial.println("End Failed"); break;
		}
	});
	ArduinoOTA.begin();
}

//void udp_log(char[] str) {
//	UDP.beginPacket("192.168.0.5", 5140);
//	UDP.write(str);
//	UDP.endPacket();
//}



MCP3425 mcp3425;

void setup() {
	Serial.begin(9600);

	pinMode(13, OUTPUT);
	digitalWrite(13, HIGH);

	Serial.println("Initializing...");
	if (startWifi(30)) {
		setup_ota();
	} else {
		Serial.println("failed to start wifi");
		ESP.restart();
	}
	digitalWrite(13, LOW);

//	wifi_set_sleep_type(LIGHT_SLEEP_T);
//	for (;;) {
//		wifi_fpm_set_sleep_type(MODEM_SLEEP_T);
//		wifi_fpm_open();
//		wifi_fpm_do_sleep(1000000);
//
//		wifi_fpm_do_wakeup();
//		wifi_fpm_close();
//		wifi_station_connect();
//	}
//	return;


	// IO4 = SDA, IO5 = SCL
	Wire.begin();
	mcp3425.configure(MCP3425::ONESHOT, MCP3425::SAMPLE_RATE_240SPS, MCP3425::PGA_GAIN_1);
}

inline float readAutoRange() {
	float adc;

	auto rate = MCP3425::SAMPLE_RATE_15SPS;

	mcp3425.configure(MCP3425::ONESHOT, rate, MCP3425::PGA_GAIN_1);
	adc = mcp3425.read();
//	Serial.print("adc(1) = ");
//	Serial.print(adc * 1000);
//	Serial.println("mV");
	if (adc > mcp3425.range() * 0.25) return adc;

	mcp3425.configure(MCP3425::ONESHOT, rate, MCP3425::PGA_GAIN_2);
	adc = mcp3425.read();
//	Serial.print("adc(2) = ");
//	Serial.print(adc * 1000);
//	Serial.println("mV");
	if (adc > mcp3425.range() * 0.25) return adc;

	mcp3425.configure(MCP3425::ONESHOT, rate, MCP3425::PGA_GAIN_4);
	adc = mcp3425.read();
//	Serial.print("adc(4) = ");
//	Serial.print(adc * 1e6);
//	Serial.println("uV");
	if (adc > mcp3425.range() * 0.25) return adc;

	mcp3425.configure(MCP3425::ONESHOT, rate, MCP3425::PGA_GAIN_8);
	adc = mcp3425.read();
//	Serial.print("adc(8) = ");
//	Serial.print(adc * 1e6);
//	Serial.println("uV");

	return adc;
}

void loop() {
	Serial.println("loop");
	delay(3000);

	return;
	ArduinoOTA.handle();

	float adc = readAutoRange();
//	mcp3425.configure(MCP3425::ONESHOT, MCP3425::SAMPLE_RATE_15SPS, MCP3425::PGA_GAIN_1);
//	float adc = mcp3425.read();
	Serial.print("adc(auto) = ");
	Serial.print(adc * 1000);
	Serial.print("mV ");
	Serial.print(adc * 1000000);
	Serial.println("uV");

	constexpr unsigned TURNS_N = 2000;
	constexpr unsigned LOAD_RESISTER = 10;

	float I = adc * (TURNS_N / (0.9 * LOAD_RESISTER));
	Serial.print("I0 = ");
	Serial.print(I);
	Serial.println("A");
	Serial.print("W = ");
	Serial.print(I * 100);
	Serial.println("W");

/*
	// 10bit / 0 - 1.0V
	// E(V) = analogRead(A0) / (1<<10) * 1.0
	uint16_t read = analogRead(A0);
//	float v = (float)read / (1<<10) * 1.0;
//	Serial.printf("analogRead = %u\n", read);
//	Serial.println(String(v));

	digitalWrite(13, HIGH);
	post_to_gf("/home/test/espadc", read);
	digitalWrite(13, LOW);
*/

	digitalWrite(13, HIGH);
	delay(1000);
	digitalWrite(13, LOW);
	delay(3000);
}


void serialEvent() {
	while (Serial.available()) {
		char inChar = (char)Serial.read();
		Serial.print(inChar);
	}
}
