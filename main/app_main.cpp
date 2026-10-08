#include "ina228.hpp"
#include "pm_core.hpp"
#include "pm_modbus.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstddef>
#include <cstdint>

namespace {
#if CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS
constexpr bool kOutputsCompiledIn = true;
#else
constexpr bool kOutputsCompiledIn = false;
#endif
constexpr const char *TAG = "power_manager";
constexpr gpio_num_t kRelays[] = {GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18};
constexpr uart_port_t kRtuUart = UART_NUM_2;
constexpr uint32_t kSamplePeriodMs = 25;
constexpr int64_t kRxGapUs = 20000;  // Discard incomplete frames after 20 ms.

uint32_t milliseconds() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000u);
}

void initialize_outputs_off() {
  for (const auto pin : kRelays) {
    ESP_ERROR_CHECK(gpio_set_level(pin, 0));
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(pin);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE; // EXTERNAL pull-down REQUIRED.
    io.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_set_level(pin, 0));
  }
}

void apply_outputs(uint8_t mask) {
#if CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS
  const uint8_t effective_mask = mask;
#else
  (void)mask;
  const uint8_t effective_mask = 0;
#endif
  for (unsigned i = 0; i < 3; ++i)
    ESP_ERROR_CHECK(gpio_set_level(kRelays[i], (effective_mask >> i) & 1u));
}

void initialize_rs485() {
  uart_config_t config = {};
  config.baud_rate = 115200;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;
  ESP_ERROR_CHECK(uart_param_config(kRtuUart, &config));
  ESP_ERROR_CHECK(uart_driver_install(kRtuUart, 512, 0, 0, nullptr, 0));
  // KAmod schematic: DI=GPIO25; RO=GPIO27; DE/RE=GPIO26.
  ESP_ERROR_CHECK(uart_set_pin(kRtuUart, 25, 27, 26, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_set_mode(kRtuUart, UART_MODE_RS485_HALF_DUPLEX));
}
}  // namespace

extern "C" void app_main(void) {
  initialize_outputs_off(); // First executable action: all relays de-energized.
  const uint32_t boot_id = esp_random() | 1u; // Reboot freshness, NOT authentication.
  pm::Controller controller(boot_id, kOutputsCompiledIn);
  pm::Ina228 sensor;
  const bool sensor_ready = sensor.begin();
  if (!sensor_ready) ESP_LOGE(TAG, "INA228 initialization failed; outputs locked off");
  initialize_rs485();
  ESP_LOGI(TAG, "Power Manager V1 booted; physical relay outputs %s",
           kOutputsCompiledIn ? "ENABLED" : "LOCKED");

  uint8_t frame[pm::kMaxRtuFrame] = {};
  uint8_t incoming[64] = {};
  uint8_t response[pm::kMaxRtuFrame] = {};
  size_t used = 0;
  int64_t last_rx_us = 0;
  uint32_t last_sample_ms = milliseconds() - kSamplePeriodMs;

  for (;;) {
    uint32_t now = milliseconds();
    if (now - last_sample_ms >= kSamplePeriodMs) {
      controller.observe(sensor_ready ? sensor.read() : pm::Sample{}, now);
      last_sample_ms = now;
    }
    controller.tick(now);
    apply_outputs(controller.outputs());

    const int received = uart_read_bytes(kRtuUart, incoming, sizeof incoming,
                                         pdMS_TO_TICKS(5));
    const int64_t time_us = esp_timer_get_time();
    if (received <= 0) {
      if (used && time_us - last_rx_us > kRxGapUs) used = 0;
      continue;
    }
    if (used && time_us - last_rx_us > kRxGapUs) used = 0;
    last_rx_us = time_us;
    for (int j = 0; j < received; ++j) {
      if (used >= sizeof frame) used = 0;
      frame[used++] = incoming[j];

      size_t expected = 0;
      if (used >= 2) {
        if (frame[1] == 0x03) expected = 8;
        else if (frame[1] == 0x10 && used >= 7)
          expected = 9u + frame[6];
        else if (frame[1] != 0x10) expected = 8;
      }
      if (expected > sizeof frame) {
        used = 0;
        continue;
      }
      if (expected == 0 || used < expected) continue;
      if (used > expected) { used = 0; continue; }

      size_t response_size = 0;
      now = milliseconds();
      controller.tick(now);
      if (pm::handle_modbus(frame, used, controller, now, response,
                            sizeof response, response_size)) {
        // Apply relay state BEFORE acknowledging the accepted Modbus write.
        apply_outputs(controller.outputs());
        if (response_size) {
          uart_write_bytes(kRtuUart, response, response_size);
          uart_wait_tx_done(kRtuUart, pdMS_TO_TICKS(50));
        }
      }
      used = 0;
    }
  }
}
