#include <type_traits>
template <uint8_t int_bits, uint8_t fractional_bits, class T>
static inline float fixed_point_to_float(const T fixed) {
	static_assert(std::is_unsigned<T>::value, "argument must be unsigned");
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

