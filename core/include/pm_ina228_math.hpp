#pragma once
#include <cstdint>

namespace pm {
// INA228: the 24-bit VSHUNT/VBUS registers contain 20 bits at [23:4].
// This implementation fixes ADCRANGE=0 (312.5 nV/shunt LSB) and assumes
// the stock Adafruit 15 milliohm shunt.
inline int32_t decode_signed20(uint32_t raw24) {
  const int32_t value = static_cast<int32_t>((raw24 >> 4) & 0xFFFFFu);
  return (value & 0x80000) ? value - 0x100000 : value;
}
inline int32_t shunt_raw_to_ma(uint32_t raw24) {
  const int32_t value = decode_signed20(raw24);
  // 312.5 nV / 0.015 Ohm = 1/48 mA per LSB.
  return value >= 0 ? (value + 24) / 48 : -((-value + 24) / 48);
}
inline int32_t bus_raw_to_mv(uint32_t raw24) {
  // 195.3125 uV/LSB = 25/128 mV/LSB.
  const uint32_t value = (raw24 >> 4) & 0xFFFFFu;
  return static_cast<int32_t>((static_cast<uint64_t>(value) * 25u + 64u) / 128u);
}
}  // namespace pm
