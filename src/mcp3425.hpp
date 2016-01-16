#include <Arduino.h>
#include <Wire.h>

class MCP3425 {
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

	static constexpr uint8_t READY = 7;
public:
	static constexpr float V_REF = 2.048;

	enum Mode {
		ONESHOT = 0,
		CONTINUOUS = 1,
	};
	enum SampleRate {
		SAMPLE_RATE_240SPS = 0b00, // 12bits
		SAMPLE_RATE_60SPS = 0b01, // 14bits
		SAMPLE_RATE_15SPS = 0b10, // 16bit
	};
	enum PGAGain {
		PGA_GAIN_1 = 0b00,
		PGA_GAIN_2 = 0b01,
		PGA_GAIN_4 = 0b10,
		PGA_GAIN_8 = 0b11,
	};

	uint8_t address = 0b1101000;

	// default configuration after power on of mcp3425
	union {
		uint8_t raw = 0b10010000;
		bits8<0, 1> PGA_GAIN;
		bits8<2, 3> SAMPLE_RATE;
		bits8<4>    CONVERSION_MODE;
		bits8<7>    RDY;
	} config;
	
	MCP3425() { }

	MCP3425(const uint8_t addr) {
		address = addr;
	}

	inline float range() const {
		switch (config.PGA_GAIN) {
			case PGA_GAIN_1: return V_REF;
			case PGA_GAIN_2: return V_REF / 2;
			case PGA_GAIN_4: return V_REF / 4;
			case PGA_GAIN_8: return V_REF / 8;
		}
	}

	inline int configure(const Mode m, const SampleRate sr, const PGAGain gain) {
		config.CONVERSION_MODE = m;
		config.SAMPLE_RATE = sr;
		config.PGA_GAIN = gain;
		if (m == ONESHOT) {
			config.RDY = 1;
		}
		return configure();
	}

	inline int configure() const {
#ifdef DEBUG
		Serial.print("config = ");
		Serial.println(config.raw, HEX);
#endif
		Wire.beginTransmission(address);
		Wire.write(config.raw);
		return Wire.endTransmission();
	}

	inline float read() const {
		return read(0);
	}

	float read(const uint16_t timeout) const {
		uint8_t buf[3];
		if (config.CONVERSION_MODE == ONESHOT) {
			// write READY bit for run cycle
			int error = configure();
			if (error) {
				// bus error
#ifdef DEBUG
				Serial.print("endTransmission returns: ");
				Serial.println(error);
#endif
				return 0;
			}
		}

		uint16_t start;
		if (timeout) start = millis();

		for (;;) {
			delay(2);
			readRaw(buf, 3);
			// check ready bit
			bool isReady = ( buf[2] & (1<<READY) ) == 0;
			if (isReady) {
				break;
			}
			if (timeout) {
				if (millis() - start >= timeout) {
					// timeout
#ifdef DEBUG
					Serial.println("timeout");
#endif
					return 0;
				}
			}
		}

		uint16_t bits = buf[0] << 8 | buf[1];
#ifdef DEBUG
		Serial.print("bits = ");
		Serial.println(bits, HEX);
#endif

		float val =
			(config.SAMPLE_RATE == SAMPLE_RATE_240SPS) ? fixed_point_to_float<1, 11>(bits):
			(config.SAMPLE_RATE == SAMPLE_RATE_60SPS) ? fixed_point_to_float<1, 13>(bits):
			(config.SAMPLE_RATE == SAMPLE_RATE_15SPS) ? fixed_point_to_float<1, 15>(bits):
			0;
#ifdef DEBUG
		Serial.print("val = ");
		Serial.println(val);
#endif

		float pga =
			(config.PGA_GAIN == PGA_GAIN_1) ? 1 :
			(config.PGA_GAIN == PGA_GAIN_2) ? 2 :
			(config.PGA_GAIN == PGA_GAIN_4) ? 4 :
			(config.PGA_GAIN == PGA_GAIN_8) ? 8 :
			0;
#ifdef DEBUG
		Serial.print("pga = ");
		Serial.println(pga);
#endif

		return val * V_REF / pga;
	}

	inline void readRaw(uint8_t* const buf, const uint8_t len) const {
		uint8_t read = Wire.requestFrom(address, len);
#ifdef DEBUG
		Serial.print("read(len) = ");
		Serial.println(read);
#endif
		for (int i = 0; i < read; i++) {
			buf[i] = Wire.read();
#ifdef DEBUG
		Serial.print("read(byte) = ");
		Serial.println(buf[i], HEX);
#endif
		}
	}
};
