#include <Arduino.h>
#include <Wire.h>

class ADT7410 {
	template <uint8_t int_bits, uint8_t fractional_bits, class T>
	static inline float fixed_point_to_float(const T fixed) {
		constexpr uint8_t msb = int_bits + fractional_bits - 1;
		constexpr T mask = (T)~(((T)~(0)) << msb);
		constexpr float deno = 1<<fractional_bits;
		if (fixed & (1<<msb)) {
			// negative
			return -( (float)( (~fixed & mask) + 1) / deno);
		} else {
			// positive
			return (float)fixed / deno;
		}
	}
	template <class T, uint8_t s, uint8_t e = s>
	struct bits {
		T ref;
		static constexpr T mask = (T)(~( (T)(~0) << (e - s + 1))) << s;
		void operator=(const T val) { ref = (ref & ~mask) | ((val & (mask >> s)) << s); }
		operator T() const { return (ref & mask) >> s; }
	};
	template <uint8_t s, uint8_t e = s> using bits8 = bits<uint8_t, s, e>;

public:
	enum OperationMode {
		CONTINUOUS = 0b00,
		ONE_SHOT = 0b01,
		ONE_SPS = 0b10,
		SHUTDOWN = 0b11,
	};

	enum Resolution {
		RES_13BIT = 0,
		RES_16BIT = 1,
	};

	uint8_t address = 0x48;

	union {
		uint8_t raw = 0;
		bits8<0, 1> FAULT_QUEUE      ;
		bits8<2>    CT_PIN_POLARITY  ;
		bits8<3>    INT_PIN_POLARITY ;
		bits8<4>    INT_CT_MODE      ;
		bits8<5, 6> OPERATION_MODE   ;
		bits8<7>    RESOLUTION       ;
	} config;

	uint8_t bytes[3] = { 0, 0, 0b0000001 };

	ADT7410() { }

	int configure() const {
		Wire.beginTransmission(address);
		Wire.write(0x03);
		Wire.write(config.raw);
		return Wire.endTransmission();
	}

	int configure(OperationMode mode, Resolution res) {
		config.OPERATION_MODE = mode;
		config.RESOLUTION = res;
		return configure();
	}

	float read() {
		if (config.OPERATION_MODE == ONE_SHOT) {
			// write ONE_SHOT bit
			configure();
			delay(240);
		}

		for (;;) {
			delay(1);
			readStatus();
			if (RDY()) {
				break;
			}
		}

		readRaw();

		uint16_t bits = bytes[0]<<8 | bytes[1];
		if (config.RESOLUTION == RES_13BIT) {
			bits >>= 3;
			return fixed_point_to_float<9, 4>(bits);
		} else {
			return fixed_point_to_float<9, 7>(bits);
		}
	}

	int readRaw() {
		Wire.beginTransmission(address);
		Wire.write(0x00);
		uint8_t error = Wire.endTransmission(false);
		if (error) return error;
		uint8_t read = Wire.requestFrom(address, (uint8_t)3);
		if (read != 3) return -1;
		bytes[0] = Wire.read();
		bytes[1] = Wire.read();
		bytes[2] = Wire.read();
		return 0;
	}

	int readStatus() {
		Wire.beginTransmission(address);
		Wire.write(0x02);
		uint8_t error = Wire.endTransmission(false);
		if (error) return error;
		uint8_t read = Wire.requestFrom(address, (uint8_t)1);
		if (read != 1) return -1;
		bytes[2] = Wire.read();
		return 0;
	}

	bool T_LOW() {
		return (bytes[2] & (1<<4)) != 0;
	}

	bool T_HIGH() {
		return (bytes[2] & (1<<5)) != 0;
	}

	bool T_CRIT() {
		return (bytes[2] & (1<<6)) != 0;
	}

	bool RDY() {
		return (bytes[2] & (1<<7)) == 0;
	}

	int reset() const {
		Wire.beginTransmission(address);
		Wire.write(0x2f);
		return Wire.endTransmission();
	}

	uint8_t readId() const {
		Wire.beginTransmission(address);
		Wire.write(0x0B);
		uint8_t error = Wire.endTransmission(false);
		if (error) return error;
		uint8_t read = Wire.requestFrom(address, (uint8_t)1);
		if (read != 1) return 0;
		return Wire.read();
	}
};
