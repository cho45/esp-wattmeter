#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

extern "C" {
	#include <espnow.h>
	#include <user_interface.h>
}

#include "rtc_memory.hpp"
#include "utils.h"
#include "mcp3425.hpp"
#include "config.h"

#define WIFI_DEFAULT_CHANNEL 1

uint8_t mac[] = {0x1A,0xFE,0x34,0xEE,0x84,0x88};

struct deep_sleep_data_t {
	static constexpr uint8_t cycle = 12;

	uint16_t count = 0;
	uint8_t  send = 0;
	uint16_t battery = 0xffff;
	uint32_t data[cycle] = {0};

	void add_data(uint32_t n) {
		Serial.print("add_data(");
		Serial.print(n);
		Serial.print(") cycle = ");
		Serial.println(count);
		data[count] = n;
	}

	template <class T>
	void run_every_cycle(T func) {
		if (send) {
			send = 0;
			func();
		}

		count++;
		if (count == cycle) {
			count = 0;
			send  = 1;
		}
	}
};
rtc_memory<deep_sleep_data_t> deep_sleep_data;

struct send_data_t {
	uint32_t chipId;
	uint16_t battery;
	uint32_t data[sizeof(deep_sleep_data->data) / sizeof(uint32_t)];
};
static_assert(sizeof(send_data_t) < 256, "size over");


MCP3425 mcp3425;

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

void printMacAddress(uint8_t* macaddr) {
	Serial.print("{");
	for (int i = 0; i < 6; i++) {
		Serial.print("0x");
		Serial.print(macaddr[i], HEX);
		if (i < 5) Serial.print(',');
	}
	Serial.println("}");
}

void post_sensor_data();

constexpr uint16_t as_battery_unit(float voltage) {
	return voltage * (1<<10) / 3.0;
}

void setup() {
	pinMode(13, OUTPUT);

	uint32_t chipId = ESP.getChipId();
	Serial.begin(74880);
	Serial.print("Initializing... chipId: 0x");
	Serial.println(chipId, HEX);

	// IO4 = SDA, IO5 = SCL
	Wire.begin();

	// データ読みこみ
	if (!deep_sleep_data.read()) {
		Serial.println("system_rtc_mem_read failed");
	}

	if (deep_sleep_data->battery < as_battery_unit(2.1)) {
		Serial.println("battery is too low...");
		ESP.deepSleep(60e6, WAKE_RF_DISABLED);
		delay(60e6);
	}

	Serial.print("deep_sleep_data->count = ");
	Serial.println(deep_sleep_data->count);

	float adc = readAutoRange();
	Serial.print("adc(auto) = ");
	Serial.print(adc * 1000);
	Serial.print("mV ");
	Serial.print(adc * 1000000);
	Serial.println("uV");
	if (adc < 0) {
		adc = 0;
	}
//	constexpr unsigned TURNS_N = 2000;
//	constexpr unsigned LOAD_RESISTER = 10;
//
//	float I = adc * (TURNS_N / (0.9 * LOAD_RESISTER));
//	Serial.print("I0 = ");
//	Serial.print(I);
//	Serial.println("A");
//	Serial.print("W = ");
//	Serial.print(I * 100);
//	Serial.println("W");

	deep_sleep_data->add_data((uint32_t)(adc * 1e6));

	deep_sleep_data->run_every_cycle([&]{
		Serial.println("check battery voltage");
		uint16_t read = analogRead(A0);
		Serial.print("analogRead = ");
		Serial.println(read);
		deep_sleep_data->battery = read;

		Serial.println("send data");
		post_sensor_data();
	});

	if (!deep_sleep_data.write()) {
		Serial.print("system_rtc_mem_write failed");
	}

	const uint32_t INTERVAL = 1e6;
	if (deep_sleep_data->send) {
		ESP.deepSleep(INTERVAL, WAKE_RF_DEFAULT);
	} else {
		ESP.deepSleep(INTERVAL, WAKE_RF_DISABLED);
	}

//	esp_now_unregister_recv_cb();
//	esp_now_deinit();
}

void loop() {
}

void post_sensor_data() {
	WiFi.mode(WIFI_STA);

	uint8_t macaddr[6];
	wifi_get_macaddr(STATION_IF, macaddr);
	Serial.print("mac address (STATION_IF): ");
	printMacAddress(macaddr);

	wifi_get_macaddr(SOFTAP_IF, macaddr);
	Serial.print("mac address (SOFTAP_IF): ");
	printMacAddress(macaddr);

	if (esp_now_init() == 0) {
		Serial.println("esp_now_init() ok");
	} else {
		Serial.println("esp_now_init() failed");
		ESP.restart();
		return;
	}

	esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
	esp_now_register_recv_cb([](uint8_t *macaddr, uint8_t *data, uint8_t len) {
		Serial.println("recv_cb");

		Serial.print("mac address: ");
		printMacAddress(macaddr);

		Serial.print("data: ");
		for (int i = 0; i < len; i++) {
			Serial.print(data[i], HEX);
		}
		Serial.println("");
	});
	esp_now_register_send_cb([](uint8_t* macaddr, uint8_t status) {
		Serial.println("send_cb");

		Serial.print("mac address: ");
		printMacAddress(macaddr);

		Serial.print("status = "); Serial.println(status);
	});

	int res = esp_now_add_peer(mac, (uint8_t)ESP_NOW_ROLE_SLAVE,(uint8_t)WIFI_DEFAULT_CHANNEL, NULL, 0);

	send_data_t send_data;

	send_data.chipId = ESP.getChipId();
	send_data.battery = deep_sleep_data->battery;
	memcpy(send_data.data, deep_sleep_data->data, sizeof(send_data.data));

	Serial.print("data: ");
	for (int i = 0; i < deep_sleep_data_t::cycle; i++) {
		Serial.print(" 0x");
		Serial.println(deep_sleep_data->data[i], HEX);
	}
	Serial.println("");

	uint8_t* data = static_cast<uint8_t*>(static_cast<void*>(&send_data));

	Serial.print("data: ");
	for (int i = 0; i < sizeof(send_data); i++) {
		Serial.print(" 0x");
		Serial.print(data[i], HEX);
	}
	Serial.println("");

	Serial.flush();

	esp_now_send(mac, data, sizeof(send_data));
}
