#ifndef FP16_UTILS_H_
#define FP16_UTILS_H_

#include <cstdint>
#include <cstring>

namespace fp16_utils {

inline uint32_t round_to_even(uint32_t value, unsigned int shift) {
  const uint32_t quotient = value >> shift;
  const uint32_t remainder = value & ((uint32_t{1} << shift) - 1);
  const uint32_t halfway = uint32_t{1} << (shift - 1);
  return quotient + (remainder > halfway ||
                     (remainder == halfway && (quotient & 1U) != 0));
}

inline uint16_t from_float(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));

  const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000U);
  const uint32_t exponent = (bits >> 23) & 0xffU;
  const uint32_t fraction = bits & 0x7fffffU;

  if (exponent == 0xffU) {
    const uint16_t payload = fraction == 0
        ? 0
        : static_cast<uint16_t>((fraction >> 13) | 0x0200U);
    return static_cast<uint16_t>(sign | 0x7c00U | payload);
  }
  if (exponent == 0) {
    return sign;
  }

  const int unbiased_exponent = static_cast<int>(exponent) - 127;
  if (unbiased_exponent > 15) {
    return static_cast<uint16_t>(sign | 0x7c00U);
  }
  if (unbiased_exponent >= -14) {
    uint32_t half_exponent = static_cast<uint32_t>(unbiased_exponent + 15);
    uint32_t half_fraction = round_to_even(fraction, 13);
    if (half_fraction == 0x400U) {
      half_fraction = 0;
      ++half_exponent;
      if (half_exponent >= 0x1fU) {
        return static_cast<uint16_t>(sign | 0x7c00U);
      }
    }
    return static_cast<uint16_t>(sign | (half_exponent << 10) | half_fraction);
  }
  if (unbiased_exponent < -25) {
    return sign;
  }

  const uint32_t significand = fraction | 0x800000U;
  const unsigned int shift = static_cast<unsigned int>(-unbiased_exponent - 1);
  const uint32_t half_fraction = round_to_even(significand, shift);
  return static_cast<uint16_t>(sign | half_fraction);
}

inline float to_float(uint16_t value) {
  const uint32_t sign = static_cast<uint32_t>(value & 0x8000U) << 16;
  uint32_t exponent = (value >> 10) & 0x1fU;
  uint32_t fraction = value & 0x03ffU;
  uint32_t bits;

  if (exponent == 0) {
    if (fraction == 0) {
      bits = sign;
    } else {
      int unbiased_exponent = -14;
      while ((fraction & 0x0400U) == 0) {
        fraction <<= 1;
        --unbiased_exponent;
      }
      fraction &= 0x03ffU;
      bits = sign |
          (static_cast<uint32_t>(unbiased_exponent + 127) << 23) |
          (fraction << 13);
    }
  } else if (exponent == 0x1fU) {
    bits = sign | 0x7f800000U | (fraction << 13);
  } else {
    exponent = exponent - 15U + 127U;
    bits = sign | (exponent << 23) | (fraction << 13);
  }

  float result;
  std::memcpy(&result, &bits, sizeof(result));
  return result;
}

}  // namespace fp16_utils

template <
    typename SrcType,
    typename SrcBits,
    int srcMantissa,
    typename DstType,
    typename DstBits,
    int dstMantissa
>
DstType __extendXfYf2__(SrcType a) {
    return fp16_utils::to_float(a);
}
#endif  // FP16_UTILS_H_
