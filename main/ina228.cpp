#include "ina228.hpp"
#include "pm_ina228_math.hpp"
#include "esp_err.h"

namespace pm {
namespace {
constexpr uint8_t kInaAddress = 0x40;
constexpr uint8_t kConfig = 0x00;
constexpr uint8_t kAdcConfig = 0x01;
constexpr uint8_t kVshunt = 0x04;
constexpr uint8_t kVbus = 0x05;
constexpr uint8_t kManufacturer = 0x3E;
constexpr uint8_t kDeviceId = 0x3F;
}

bool Ina228::read_register(uint8_t reg, uint8_t *buffer, size_t count) {
  return device_ && i2c_master_transmit_receive(device_, &reg, 1, buffer, count, 50) == ESP_OK;
}
bool Ina228::read_u16(uint8_t reg, uint16_t &value) {
  uint8_t b[2] = {};
  if (!read_register(reg, b, sizeof b)) return false;
  value = static_cast<uint16_t>((static_cast<uint16_t>(b[0]) << 8) | b[1]);
  return true;
}
bool Ina228::read_u24(uint8_t reg, uint32_t &value) {
  uint8_t b[3] = {};
  if (!read_register(reg, b, sizeof b)) return false;
  value = (static_cast<uint32_t>(b[0]) << 16) |
          (static_cast<uint32_t>(b[1]) << 8) | b[2];
  return true;
}
bool Ina228::write_u16(uint8_t reg, uint16_t value) {
  uint8_t b[] = {reg, static_cast<uint8_t>(value >> 8),
                       static_cast<uint8_t>(value)};
  return device_ && i2c_master_transmit(device_, b, sizeof b, 50) == ESP_OK;
}
bool Ina228::begin() {
  i2c_master_bus_config_t config = {};
  config.i2c_port = I2C_NUM_0;
  config.sda_io_num = GPIO_NUM_33;
  config.scl_io_num = GPIO_NUM_32;
  config.clk_source = I2C_CLK_SRC_DEFAULT;
  config.glitch_ignore_cnt = 7;
  config.flags.enable_internal_pullup = false; // KAmod board already has I2C pull-ups.
  if (i2c_new_master_bus(&config, &bus_) != ESP_OK) return false;
  i2c_device_config_t device_cfg = {};
  device_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device_cfg.device_address = kInaAddress;
  device_cfg.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus_, &device_cfg, &device_) != ESP_OK) return false;

  uint16_t manufacturer = 0, id = 0;
  if (!read_u16(kManufacturer, manufacturer) || !read_u16(kDeviceId, id) ||
      manufacturer != 0x5449 || (id >> 4) != 0x228) return false;
  // Explicitly enforce ADCRANGE=0: +/-163.84 mV, 312.5 nV/LSB.
  // Use chip default continuous shunt+bus+temp mode (FB68h).
  if (!write_u16(kConfig, 0x0000) || !write_u16(kAdcConfig, 0xFB68))
    return false;
  uint16_t checked_cfg = 0, checked_adc = 0;
  return read_u16(kConfig, checked_cfg) && read_u16(kAdcConfig, checked_adc) &&
         (checked_cfg & 0x0010) == 0 && checked_adc == 0xFB68;
}
Sample Ina228::read() {
  uint32_t vbus = 0, vshunt = 0;
  if (!read_u24(kVbus, vbus) || !read_u24(kVshunt, vshunt))
    return {};
  return {true, bus_raw_to_mv(vbus), shunt_raw_to_ma(vshunt)};
}
}  // namespace pm
