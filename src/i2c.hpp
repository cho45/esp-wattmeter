
class I2CDriver {
	inline uint8_t i2cset_(uint8_t address) {
		return Wire.endTransmission();
	}

	template <class... Args>
	inline uint8_t i2cset_(uint8_t address, uint8_t data, Args... rest) {
		Wire.write(data);
		return i2cset_(address, rest...);
	}
public: 
	I2CDriver() {
		Wire.begin();
	}

	inline uint8_t i2cget(uint8_t address, uint8_t param, uint8_t length, uint8_t* &ret) {
		Wire.beginTransmission(address);
		Wire.write(param);
		uint8_t error = Wire.endTransmission(false);
		if (error) {
			return 0;
		}
		uint8_t read = Wire.requestFrom(address, length);
		for (uint8_t i = 0; i < length; i++) {
			ret[i] = Wire.read();
		}
		return read;
	}

	template <class... Args>
	inline uint8_t i2cset(uint8_t address, Args... rest) {
		Wire.beginTransmission(address);
		return i2cset_(address, rest...);
	}

};
