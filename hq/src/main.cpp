#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
	#include <espnow.h>
	#include <user_interface.h>
}

// platformio lib install 66
#include <HttpClient.h>
#include <deque>

#include "gf.hpp"
#include "config.h"

WiFiClient wifiClient;
HttpClient http(wifiClient);
GrowthForecastClient gf(http, GF_HOST, GF_USER, GF_PASS);

struct {
	char SSID[255] = WIFI_SSID;
	char Password[255] = WIFI_PASS;
}  wifi_config;

#define WIFI_DEFAULT_CHANNEL 1

uint8_t mac[] = {0x5C,0xCF,0x7F,0x8,0x37,0xC7};

struct send_data_t {
	uint32_t chipId;
	uint16_t battery;
	uint32_t data[12];
};

std::deque< send_data_t > data_queue;

void printMacAddress(uint8_t* macaddr) {
	Serial.print("{");
	for (int i = 0; i < 6; i++) {
		Serial.print("0x");
		Serial.print(macaddr[i], HEX);
		if (i < 5) Serial.print(',');
	}
	Serial.println("}");
}

bool startWifi(int timeout) {
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
		delay(1000);
		Serial.print(".");
		time++;
		if (time >= timeout) {
			break;
		}
	}
	return false;
}

void setup() {
	pinMode(13, OUTPUT);

	Serial.begin(74880);
	Serial.println("Initializing...");

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP("foobar", "12345678", 1, 0);

	if (startWifi(30)) {
	} else {
		Serial.println("failed to start wifi");
		ESP.restart();
	}

	uint8_t macaddr[6];
	wifi_get_macaddr(STATION_IF, macaddr);
	Serial.print("mac address (STATION_IF): ");
	printMacAddress(macaddr);

	wifi_get_macaddr(SOFTAP_IF, macaddr);
	Serial.print("mac address (SOFTAP_IF): ");
	printMacAddress(macaddr);

	if (esp_now_init() == 0) {
		Serial.println("init");
	} else {
		Serial.println("init failed");
		ESP.restart();
		return;
	}

	esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
	esp_now_register_recv_cb([](uint8_t *macaddr, uint8_t *data, uint8_t len) {
//		Serial.print("data: ");
//		for (int i = 0; i < len; i++) {
//			Serial.print(" 0x");
//			Serial.print(data[i], HEX);
//		}
//
//		Serial.println("");
//		Serial.print("recv_cb length=");
//		Serial.print(len);
//		Serial.print(" from mac address: ");
//		printMacAddress(macaddr);
//

		// exception...
		//send_data_t* send_data = static_cast<send_data_t*>(static_cast<void*>(data));

		send_data_t send_data;
		memcpy(&send_data, data, sizeof(send_data));

//		uint8_t data_len = (len - sizeof(send_data.chipId) - sizeof(send_data.battery)) / sizeof(uint32_t);
//		Serial.print("data_len: ");
//		Serial.println(data_len);

		data_queue.push_back(send_data);

	});
	esp_now_register_send_cb([](uint8_t* macaddr, uint8_t status) {
		Serial.println("send_cb");

		Serial.print("mac address: ");
		printMacAddress(macaddr);

		Serial.print("status = "); Serial.println(status);
	});

	int res = esp_now_add_peer(mac, (uint8_t)ESP_NOW_ROLE_CONTROLLER,(uint8_t)WIFI_DEFAULT_CHANNEL, NULL, 0);

//	esp_now_unregister_recv_cb();
//	esp_now_deinit();
}

void loop() {
	Serial.println("loop");
	while (!data_queue.empty()) {
		Serial.println("shift queue");
		send_data_t send_data = data_queue.front();
		data_queue.pop_front();

		Serial.print("chipId: ");
		Serial.println(send_data.chipId, HEX);

		Serial.print("battery: ");
		Serial.print( static_cast<float>(send_data.battery) / (1<<10) * 3);
		Serial.print(" V (");
		Serial.print(send_data.battery);
		Serial.println(")");

		float avg = 0;
		for (int i = 0; i < 12; i++) {
//			Serial.print(" 0x");
//			Serial.print(send_data.data[i], HEX);
			float adc = send_data.data[i] / 1e6f;
			constexpr unsigned TURNS_N = 2000;
			constexpr unsigned LOAD_RESISTER = 10;

			float I = adc * (TURNS_N / (0.9 * LOAD_RESISTER));
			Serial.print(I * 100);
			Serial.println("W");
			avg += I;
		}
		Serial.println("");
		avg = avg / 12.0;

		Serial.print("avg = ");
		Serial.println(avg * 100);

		gf.post("/home/test/watt", (int32_t)(avg * 100));
	}
	delay(1000);
}

