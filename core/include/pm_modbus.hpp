#pragma once
#include "pm_core.hpp"
#include <cstddef>
#include <cstdint>

namespace pm {
constexpr uint8_t kModbusAddress = 0x01;
constexpr size_t kMaxRtuFrame = 128;
uint16_t modbus_crc(const uint8_t *data, size_t length);

// Complete RTU requests only. false means ignore frame (invalid CRC, address,
// or framing). true means a complete response was produced, including exceptions.
bool handle_modbus(const uint8_t *request, size_t length, Controller &controller,
                   uint32_t now_ms, uint8_t *response, size_t response_capacity,
                   size_t &response_length);
}  // namespace pm
