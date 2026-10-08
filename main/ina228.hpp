#pragma once
#include "pm_core.hpp"
#include "driver/i2c_master.h"
#include <cstddef>
#include <cstdint>

namespace pm {
class Ina228 {
 public:
  // Checks TI manufacturer/device IDs, sets ADCRANGE=0 and continuous ADC mode.
  bool begin();
  Sample read();
 private:
  bool read_register(uint8_t reg, uint8_t *buffer, size_t count);
  bool read_u16(uint8_t reg, uint16_t &value);
  bool read_u24(uint8_t reg, uint32_t &value);
  bool write_u16(uint8_t reg, uint16_t value);
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
};
}  // namespace pm
