#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace victron_charge_limit {

/**
 * Victron VE.Direct Charge Current Limit Component
 *
 * Writes to register 0x2015 (Charge Current Limit) via VE.Direct HEX protocol.
 * This register is specifically for dynamic/remote current limiting and
 * should be used instead of 0xEDF0 for frequent updates (to avoid flash wear).
 *
 * Register: 0x2015
 * Data type: un16 (unsigned 16-bit)
 * Unit: 0.1 A (value 125 = 12.5 A)
 * Access: Read/Write
 */
class VictronChargeLimit : public number::Number, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

  // VE.Direct HEX protocol helpers
  void send_hex_set_command(uint16_t reg_id, uint16_t value);
  void send_hex_get_command(uint16_t reg_id);
  uint8_t calculate_checksum(const uint8_t *data, size_t len);
  void parse_hex_response();

  // Buffer for incoming data
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_request_time_{0};
  bool waiting_for_response_{false};

  // Current value cache
  float current_value_{0.0f};

  // Register address for charge current limit
  static const uint16_t REG_CHARGE_CURRENT_LIMIT = 0x2015;
};

}  // namespace victron_charge_limit
}  // namespace esphome
