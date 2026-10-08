#include "pm_core.hpp"
#include <cstdint>

namespace pm {
namespace {
uint32_t age(uint32_t now, uint32_t before) { return now - before; }
int64_t magnitude(int32_t value) {
  return value < 0 ? -static_cast<int64_t>(value) : static_cast<int64_t>(value);
}
}

Controller::Controller(uint32_t boot_id, bool physical_outputs_enabled)
    : boot_id_(boot_id), outputs_enabled_(physical_outputs_enabled) {}

void Controller::trip(uint16_t fault) {
  faults_ = static_cast<uint16_t>(faults_ | fault);
  outputs_ = 0;
  state_ = State::Fault;
}

bool Controller::fresh(uint32_t now_ms) const {
  return sample_valid_ && age(now_ms, last_sample_ms_) <= kSampleMaxAgeMs;
}

bool Controller::readings_safe(uint32_t now_ms) const {
  return fresh(now_ms) && bus_mv_ >= 0 && bus_mv_ <= kMaxVoltageMv &&
         magnitude(current_ma_) <= kMaxCurrentMa;
}

void Controller::observe(const Sample &sample, uint32_t now_ms) {
  if (!started_) { started_ = true; started_ms_ = now_ms; }
  if (!sample.valid) {
    sample_valid_ = false;
    trip(SensorFault);
    return;
  }
  sample_valid_ = true;
  bus_mv_ = sample.bus_mv;
  current_ma_ = sample.current_ma;
  last_sample_ms_ = now_ms;
  if (bus_mv_ < 0 || bus_mv_ > kMaxVoltageMv) trip(OverVoltage);
  if (magnitude(current_ma_) > kMaxCurrentMa) trip(OverCurrent);
}

void Controller::tick(uint32_t now_ms) {
  if (!started_) { started_ = true; started_ms_ = now_ms; }
  if (state_ == State::Armed) {
    if (age(now_ms, last_lease_ms_) > kLeaseMs) trip(CommunicationTimeout);
    if (!fresh(now_ms)) trip(SensorStale);
  }
}

bool Controller::command(const Command &cmd, uint32_t now_ms) {
  tick(now_ms);
  // Safety-off deliberately works even with an obsolete boot token or sequence.
  if (cmd.operation == Op::EmergencyOff && cmd.output_mask == 0) {
    outputs_ = 0;
    state_ = State::SafeOff;
    return true;
  }
  if (cmd.boot_id != boot_id_ ||
      cmd.sequence != static_cast<uint16_t>(last_sequence_ + 1u)) return false;
  if (cmd.operation != Op::SetOutputs && cmd.output_mask != 0) return false;
  if (cmd.operation == Op::SetOutputs && (cmd.output_mask & ~0x0007u)) return false;

  switch (cmd.operation) {
    case Op::Arm:
      if (!outputs_enabled_ || state_ != State::SafeOff || faults_ ||
          !readings_safe(now_ms)) return false;
      state_ = State::Armed;
      last_lease_ms_ = now_ms;
      break;
    case Op::SetOutputs:
      if (state_ != State::Armed || !readings_safe(now_ms)) return false;
      outputs_ = static_cast<uint8_t>(cmd.output_mask);
      last_lease_ms_ = now_ms;
      break;
    case Op::Keepalive:
      if (state_ != State::Armed || !readings_safe(now_ms)) return false;
      last_lease_ms_ = now_ms;
      break;
    case Op::ClearFault:
      if (state_ != State::Fault || !readings_safe(now_ms)) return false;
      faults_ = 0;
      outputs_ = 0;
      state_ = State::SafeOff;
      break;
    case Op::Disarm:
      outputs_ = 0;
      state_ = State::SafeOff;
      break;
    default:
      return false;
  }
  last_sequence_ = cmd.sequence;
  return true;
}

uint16_t Controller::holding(uint16_t address, uint32_t now_ms) const {
  const uint32_t voltage = static_cast<uint32_t>(bus_mv_);
  const uint32_t current = static_cast<uint32_t>(current_ma_);
  const int64_t power64 = static_cast<int64_t>(bus_mv_) * current_ma_ / 1000;
  const uint32_t power = static_cast<uint32_t>(static_cast<int32_t>(power64));
  const uint32_t uptime_s = started_ ? age(now_ms, started_ms_) / 1000 : 0;
  const uint32_t measurement_age = sample_valid_ ? age(now_ms, last_sample_ms_) : 0xFFFFu;
  const uint32_t remaining =
      (state_ == State::Armed && age(now_ms, last_lease_ms_) < kLeaseMs)
      ? kLeaseMs - age(now_ms, last_lease_ms_) : 0;
  switch (address) {
    case 0x00: return kProtocolVersion;
    case 0x01: return static_cast<uint16_t>(state_);
    case 0x02: return outputs_;
    case 0x03: return faults_;
    case 0x04: return upper32(voltage);
    case 0x05: return lower32(voltage);
    case 0x06: return upper32(current);
    case 0x07: return lower32(current);
    case 0x08: return upper32(power);
    case 0x09: return lower32(power);
    case 0x0A: return measurement_age > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(measurement_age);
    case 0x0B: return upper32(uptime_s);
    case 0x0C: return lower32(uptime_s);
    case 0x0D: return static_cast<uint16_t>((sample_valid_ ? 1 : 0) |
                   (outputs_enabled_ ? 2 : 0) | (fresh(now_ms) ? 4 : 0));
    case 0x0E: return last_sequence_;
    case 0x0F: return static_cast<uint16_t>(kMaxCurrentMa);
    case 0x10: return upper32(boot_id_);
    case 0x11: return lower32(boot_id_);
    case 0x12: return static_cast<uint16_t>(kMaxVoltageMv);
    case 0x13: return static_cast<uint16_t>(remaining);
    default: return 0;
  }
}
}  // namespace pm
