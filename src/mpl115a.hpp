#include <Arduino.h>
#include <Wire.h>

class MPL115A {
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

public:
	uint8_t address = 0x60;

	float a0;
	float b1;
	float b2;
	float c12;

	MPL115A() { }

	MPL115A(uint8_t addr) {
		address = addr;
	}

	inline int initCoefficient() {
		Wire.beginTransmission(address);
		Wire.write(0x04);
		Wire.endTransmission(false);
		uint8_t read = Wire.requestFrom(address, (uint8_t)8);
#ifdef DEBUG
		Serial.print("read = ");
		Serial.println(read);
#endif
		if (read != 8) return -1;

		uint16_t i_a0  = Wire.read()<<8 | Wire.read();
		uint16_t i_b1  = Wire.read()<<8 | Wire.read();
		uint16_t i_b2  = Wire.read()<<8 | Wire.read();
		uint16_t i_c12 = Wire.read()<<8 | Wire.read();

		a0  = fixed_point_to_float<13, 3>(i_a0);
		b1  = fixed_point_to_float<3, 13>(i_b1);
		b2  = fixed_point_to_float<2, 14>(i_b2);
		c12 = fixed_point_to_float<1, 15>(i_c12)/(1<<9);
		return 0;
	}

	inline float calc_hPa() {
		Wire.beginTransmission(address);
		Wire.write(0x12);
		Wire.write(0x01);
		Wire.endTransmission();

		delay(3);

		Wire.beginTransmission(address);
		Wire.write(0x00);
		uint8_t error = Wire.endTransmission(false);
		if (error) return error;
		uint8_t read = Wire.requestFrom(address, (uint8_t)4);
		if (read != 4) return 0;

		uint16_t p_adc = Wire.read()<<8 | Wire.read();
		uint16_t t_adc = Wire.read()<<8 | Wire.read();

		Serial.print("p_adc = "); Serial.println(p_adc, HEX);
		Serial.print("t_adc = "); Serial.println(t_adc, HEX);

		p_adc >>= 6;
		t_adc >>= 6;

		float c12x2 = c12 * t_adc;
		float a1 = b1 + c12x2;
		float a1x1 = a1 * p_adc;
		float y1 = a0 + a1x1;
		float a2x2 = b2 * t_adc;
		float p_comp = y1 + a2x2;

		// float p_comp = a0 + (b1 + c12 * t_adc) * p_adc + b2 * t_adc;
		float hPa = p_comp * ( (1150 - 500) / 1023.0) + 500;
		return hPa;
	}
};
