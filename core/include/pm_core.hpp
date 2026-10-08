#pragma once
#include <cstdint>

namespace pm {

constexpr uint16_t kProtocolVersion = 0x0100;
constexpr uint16_t kRegisterCount = 0x0014;
constexpr uint16_t kCommandAddress = 0x0100;
constexpr uint16_t kCommandWords = 5;
constexpr uint32_t kLeaseMs = 1500;
constexpr uint32_t kSampleMaxAgeMs = 300;
constexpr int32_t kMaxVoltageMv = 28000;
constexpr int32_t kMaxCurrentMa = 2000;  // Conservative laboratory default; not a fuse.

enum class State : uint16_t { SafeOff = 0, Armed = 1, Fault = 2 };
enum class Op : uint16_t { EmergencyOff = 0, Arm = 1, SetOutputs = 2, Keepalive = 3, ClearFault = 4, Disarm = 5 };
enum Fault : uint16_t {
  SensorFault = 1u << 0,
  OverVoltage = 1u << 1,
  OverCurrent = 1u << 2,
  CommunicationTimeout = 1u << 3,
  SensorStale = 1u << 4
};

struct Sample {
  bool valid = false;
  int32_t bus_mv = 0;
  int32_t current_ma = 0;
};

struct Command {
  uint32_t boot_id = 0;
  uint16_t sequence = 0;
  Op operation = Op::EmergencyOff;
  uint16_t output_mask = 0;
};

class Controller {
 public:
  explicit Controller(uint32_t boot_id, bool physical_outputs_enabled = false);
  void observe(const Sample &sample, uint32_t now_ms);
  void tick(uint32_t now_ms);
  bool command(const Command &command, uint32_t now_ms);
  uint16_t holding(uint16_t address, uint32_t now_ms) const;
  State state() const { return state_; }
  uint8_t outputs() const { return outputs_; }
  uint16_t faults() const { return faults_; }
  bool fresh(uint32_t now_ms) const;
 private:
  void trip(uint16_t fault);
  bool readings_safe(uint32_t now_ms) const;
  static uint16_t upper32(uint32_t value) { return static_cast<uint16_t>(value >> 16); }
  static uint16_t lower32(uint32_t value) { return static_cast<uint16_t>(value); }
  const uint32_t boot_id_;
  const bool outputs_enabled_;
  State state_ = State::SafeOff;
  uint8_t outputs_ = 0;
  uint16_t faults_ = 0;
  uint16_t last_sequence_ = 0;
  bool sample_valid_ = false;
  int32_t bus_mv_ = 0;
  int32_t current_ma_ = 0;
  uint32_t last_sample_ms_ = 0;
  uint32_t last_lease_ms_ = 0;
  uint32_t started_ms_ = 0;
  bool started_ = false;
};

}  // namespace pm
