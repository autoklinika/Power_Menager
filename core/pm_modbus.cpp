#include "pm_modbus.hpp"

namespace pm {
namespace {
uint16_t be16(const uint8_t *data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}
void append_crc(uint8_t *buffer, size_t &n) {
  const uint16_t crc = modbus_crc(buffer, n);
  buffer[n++] = static_cast<uint8_t>(crc & 0xFF);
  buffer[n++] = static_cast<uint8_t>(crc >> 8);
}
bool exception(uint8_t addr, uint8_t function, uint8_t error,
               uint8_t *response, size_t capacity, size_t &out) {
  if (capacity < 5) return false;
  response[0] = addr;
  response[1] = static_cast<uint8_t>(function | 0x80);
  response[2] = error;
  out = 3;
  append_crc(response, out);
  return true;
}
}  // namespace

uint16_t modbus_crc(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = static_cast<uint16_t>((crc & 1u) ? (crc >> 1) ^ 0xA001u : crc >> 1);
  }
  return crc;
}

bool handle_modbus(const uint8_t *request, size_t length, Controller &controller,
                   uint32_t now_ms, uint8_t *response, size_t capacity, size_t &out) {
  out = 0;
  if (!request || !response || length < 8 || length > kMaxRtuFrame) return false;
  if (request[0] != kModbusAddress) return false;
  const uint16_t supplied_crc = static_cast<uint16_t>(request[length - 2] |
                                               (request[length - 1] << 8));
  if (modbus_crc(request, length - 2) != supplied_crc) return false;
  const uint8_t fc = request[1];
  if (fc == 0x03) {
    if (length != 8) return false;
    const uint16_t first = be16(request + 2), count = be16(request + 4);
    if (!count || count > kRegisterCount ||
        first >= kRegisterCount || static_cast<uint32_t>(first) + count > kRegisterCount)
      return exception(request[0], fc, 0x02, response, capacity, out);
    if (capacity < static_cast<size_t>(5u + count * 2u)) return false;
    response[0] = request[0]; response[1] = fc;
    response[2] = static_cast<uint8_t>(count * 2u);
    out = 3;
    for (uint16_t offset = 0; offset < count; ++offset) {
      const uint16_t value = controller.holding(first + offset, now_ms);
      response[out++] = static_cast<uint8_t>(value >> 8);
      response[out++] = static_cast<uint8_t>(value);
    }
    append_crc(response, out);
    return true;
  }
  if (fc == 0x10) {
    if (length < 9 || length != static_cast<size_t>(9u + request[6])) return false;
    const uint16_t first = be16(request + 2), count = be16(request + 4);
    if (first != kCommandAddress || count != kCommandWords ||
        request[6] != kCommandWords * 2)
      return exception(request[0], fc, 0x02, response, capacity, out);
    Command cmd;
    cmd.boot_id = (static_cast<uint32_t>(be16(request + 7)) << 16) | be16(request + 9);
    cmd.sequence = be16(request + 11);
    cmd.operation = static_cast<Op>(be16(request + 13));
    cmd.output_mask = be16(request + 15);
    if (!controller.command(cmd, now_ms))
      return exception(request[0], fc, 0x03, response, capacity, out);
    if (capacity < 8) return false;
    for (size_t i = 0; i < 6; ++i) response[i] = request[i];
    out = 6;
    append_crc(response, out);
    return true;
  }
  return exception(request[0], fc, 0x01, response, capacity, out);
}
}  // namespace pm
