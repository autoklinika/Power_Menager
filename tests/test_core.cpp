#include "pm_core.hpp"
#include "pm_modbus.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace pm;

namespace {
std::vector<uint8_t> with_crc(std::vector<uint8_t> bytes) {
  const uint16_t crc = modbus_crc(bytes.data(), bytes.size());
  bytes.push_back(static_cast<uint8_t>(crc));
  bytes.push_back(static_cast<uint8_t>(crc >> 8));
  return bytes;
}
std::vector<uint8_t> command_frame(uint32_t token, uint16_t seq, uint16_t op, uint16_t mask) {
  return with_crc({1, 0x10, 1, 0, 0, 5, 10,
      static_cast<uint8_t>(token >> 24), static_cast<uint8_t>(token >> 16),
      static_cast<uint8_t>(token >> 8), static_cast<uint8_t>(token),
      static_cast<uint8_t>(seq >> 8), static_cast<uint8_t>(seq),
      static_cast<uint8_t>(op >> 8), static_cast<uint8_t>(op),
      static_cast<uint8_t>(mask >> 8), static_cast<uint8_t>(mask)});
}
std::vector<uint8_t> reply(const std::vector<uint8_t>& frame, Controller &ctrl,
                           uint32_t now) {
  uint8_t buffer[128] = {};
  size_t n = 0;
  const bool handled = handle_modbus(frame.data(), frame.size(), ctrl, now,
                                    buffer, sizeof buffer, n);
  assert(handled);
  return std::vector<uint8_t>(buffer, buffer + n);
}
}

int main() {
  // Well-known Modbus RTU CRC vector.
  const uint8_t vec[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  assert(modbus_crc(vec, sizeof vec) == 0x0A84);

  constexpr uint32_t nonce = 0xABCD1234;
  const Sample good{true, 24000, 400};
  Controller locked(nonce, false);
  locked.observe(good, 10);
  assert(!locked.command({nonce, 1, Op::Arm, 0}, 20));
  assert(locked.outputs() == 0);

  Controller ctrl(nonce, true);
  assert(ctrl.state() == State::SafeOff);
  assert(!ctrl.command({nonce, 1, Op::Arm, 0}, 1));  // No sensor.
  ctrl.observe(good, 10);
  assert(!ctrl.command({0x99999999, 1, Op::Arm, 0}, 11));
  assert(!ctrl.command({nonce, 2, Op::Arm, 0}, 11)); // Replay/sequence gap.
  assert(ctrl.command({nonce, 1, Op::Arm, 0}, 12));
  assert(ctrl.state() == State::Armed && ctrl.outputs() == 0);
  assert(!ctrl.command({nonce, 2, Op::SetOutputs, 8}, 20));
  assert(ctrl.command({nonce, 2, Op::SetOutputs, 5}, 20));
  assert(ctrl.outputs() == 5);
  assert(!ctrl.command({nonce, 2, Op::SetOutputs, 7}, 21));
  assert(ctrl.command({nonce, 3, Op::Keepalive, 0}, 100));
  ctrl.observe({true, 24000, 2500}, 120);
  assert(ctrl.state() == State::Fault && ctrl.outputs() == 0);
  assert(ctrl.faults() & OverCurrent);
  assert(!ctrl.command({nonce, 4, Op::ClearFault, 0}, 121));
  ctrl.observe(good, 122);
  assert(ctrl.command({nonce, 4, Op::ClearFault, 0}, 123));
  assert(ctrl.state() == State::SafeOff && ctrl.faults() == 0);
  assert(ctrl.command({nonce, 5, Op::Arm, 0}, 124));
  ctrl.tick(1625);
  assert(ctrl.state() == State::Fault && ctrl.outputs() == 0);
  assert(ctrl.faults() & CommunicationTimeout);
  assert(ctrl.faults() & SensorStale);

  // Nonce and sequence required for all energizing operations.
  Controller rt(nonce, true);
  rt.observe(good, 10);
  auto bad = command_frame(nonce, 1, 1, 0);
  bad.back() ^= 0xFF;
  uint8_t output[128] = {};
  size_t output_size = 0;
  assert(!handle_modbus(bad.data(), bad.size(), rt, 20, output, sizeof output, output_size));
  assert(rt.state() == State::SafeOff);
  auto reply_arm = reply(command_frame(nonce, 1, 1, 0), rt, 20);
  assert(reply_arm.size() == 8 && reply_arm[1] == 0x10);
  auto reply_set = reply(command_frame(nonce, 2, 2, 7), rt, 25);
  assert(reply_set[1] == 0x10 && rt.outputs() == 7);
  auto replay = reply(command_frame(nonce, 2, 2, 7), rt, 26);
  assert(replay.size() == 5 && replay[1] == 0x90 && replay[2] == 0x03);

  const auto read = reply(with_crc({1, 3, 0, 0, 0, 20}), rt, 40);
  assert(read.size() == 45 && read[1] == 3 && read[2] == 40);
  assert(read[3] == 0x01 && read[4] == 0x00);
  auto invalid_read = reply(with_crc({1, 3, 0, 19, 0, 2}), rt, 50);
  assert(invalid_read[1] == 0x83 && invalid_read[2] == 0x02);

  auto emergency = reply(command_frame(0, 0, 0, 0), rt, 60);
  assert(emergency[1] == 0x10 && rt.outputs() == 0);
  assert(rt.state() == State::SafeOff);
  rt.observe({false, 0, 0}, 70);
  assert(rt.state() == State::Fault);
  assert(!rt.command({nonce, 3, Op::Arm, 0}, 71));
  auto emergency_after_fault = reply(command_frame(0, 0, 0, 0), rt, 72);
  assert(emergency_after_fault[1] == 0x10);
  assert(rt.state() == State::Fault); // Must not hide or erase latched faults.

  Controller volt(nonce, true);
  volt.observe({true, 29000, 0}, 0);
  assert(volt.faults() & OverVoltage);
  Controller stale(nonce, true);
  stale.observe(good, 0);
  assert(stale.command({nonce, 1, Op::Arm, 0}, 1));
  stale.tick(302);
  assert(stale.faults() & SensorStale);

  std::cout << "Power Manager host tests: PASS\n";
}
