#include "victron_charge_limit.h"
#include "esphome/core/log.h"

namespace esphome {
namespace victron_charge_limit {

static const char *const TAG = "victron_charge_limit";

// VE.Direct HEX protocol constants
static const uint8_t HEX_RECORD_START = ':';
static const uint8_t HEX_RECORD_END = '\n';
static const uint8_t HEX_CMD_GET = 0x07;
static const uint8_t HEX_CMD_SET = 0x08;
static const uint8_t HEX_CHECKSUM_TARGET = 0x55;

void VictronChargeLimit::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Victron Charge Current Limit...");

  // Request current value on startup
  this->send_hex_get_command(REG_CHARGE_CURRENT_LIMIT);
  this->last_request_time_ = millis();
  this->waiting_for_response_ = true;
}

void VictronChargeLimit::loop() {
  // Read any available data
  while (this->available()) {
    uint8_t c = this->read();

    if (c == HEX_RECORD_START) {
      // Start of new HEX frame
      this->rx_buffer_.clear();
      this->rx_buffer_.push_back(c);
    } else if (!this->rx_buffer_.empty()) {
      this->rx_buffer_.push_back(c);

      if (c == HEX_RECORD_END) {
        // End of HEX frame, parse it
        this->parse_hex_response();
        this->rx_buffer_.clear();
      }
    }

    // Prevent buffer overflow
    if (this->rx_buffer_.size() > 100) {
      this->rx_buffer_.clear();
    }
  }

  // Timeout for response
  if (this->waiting_for_response_ && (millis() - this->last_request_time_ > 5000)) {
    ESP_LOGW(TAG, "Timeout waiting for response");
    this->waiting_for_response_ = false;
  }
}

void VictronChargeLimit::dump_config() {
  ESP_LOGCONFIG(TAG, "Victron Charge Current Limit:");
  ESP_LOGCONFIG(TAG, "  Register: 0x%04X", REG_CHARGE_CURRENT_LIMIT);
  LOG_NUMBER("  ", "Charge Current Limit", this);
}

void VictronChargeLimit::control(float value) {
  ESP_LOGI(TAG, "Setting charge current limit to %.1f A", value);

  // Convert to 0.1 A units (un16)
  uint16_t raw_value = static_cast<uint16_t>(value * 10.0f);

  // Send SET command to register 0x2015
  this->send_hex_set_command(REG_CHARGE_CURRENT_LIMIT, raw_value);

  // Update state optimistically
  this->publish_state(value);
  this->current_value_ = value;
}

void VictronChargeLimit::send_hex_set_command(uint16_t reg_id, uint16_t value) {
  // VE.Direct HEX SET command format:
  // :8IILLHHCCNN
  // 8 = SET command
  // II = Register ID low byte
  // LL = Register ID high byte
  // HH = Flags (0x00)
  // CC = Value low byte
  // NN = Value high byte + checksum

  uint8_t data[7];
  data[0] = HEX_CMD_SET;                    // Command: SET
  data[1] = reg_id & 0xFF;                  // Register ID low
  data[2] = (reg_id >> 8) & 0xFF;           // Register ID high
  data[3] = 0x00;                           // Flags
  data[4] = value & 0xFF;                   // Value low (little endian)
  data[5] = (value >> 8) & 0xFF;            // Value high

  // Calculate checksum: sum of all bytes must equal 0x55
  uint8_t sum = 0;
  for (int i = 0; i < 6; i++) {
    sum += data[i];
  }
  data[6] = (HEX_CHECKSUM_TARGET - sum) & 0xFF;

  // Convert to ASCII hex and send
  char hex_str[20];
  hex_str[0] = ':';
  for (int i = 0; i < 7; i++) {
    sprintf(&hex_str[1 + i * 2], "%02X", data[i]);
  }
  hex_str[15] = '\n';
  hex_str[16] = '\0';

  ESP_LOGD(TAG, "Sending HEX command: %s", hex_str);
  this->write_str(hex_str);

  this->last_request_time_ = millis();
  this->waiting_for_response_ = true;
}

void VictronChargeLimit::send_hex_get_command(uint16_t reg_id) {
  // VE.Direct HEX GET command format:
  // :7IILLHHCC
  // 7 = GET command
  // II = Register ID low byte
  // LL = Register ID high byte
  // HH = Flags (0x00)
  // CC = Checksum

  uint8_t data[5];
  data[0] = HEX_CMD_GET;                    // Command: GET
  data[1] = reg_id & 0xFF;                  // Register ID low
  data[2] = (reg_id >> 8) & 0xFF;           // Register ID high
  data[3] = 0x00;                           // Flags

  // Calculate checksum
  uint8_t sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += data[i];
  }
  data[4] = (HEX_CHECKSUM_TARGET - sum) & 0xFF;

  // Convert to ASCII hex and send
  char hex_str[14];
  hex_str[0] = ':';
  for (int i = 0; i < 5; i++) {
    sprintf(&hex_str[1 + i * 2], "%02X", data[i]);
  }
  hex_str[11] = '\n';
  hex_str[12] = '\0';

  ESP_LOGD(TAG, "Sending HEX GET command: %s", hex_str);
  this->write_str(hex_str);
}

void VictronChargeLimit::parse_hex_response() {
  if (this->rx_buffer_.size() < 4) {
    return;
  }

  // Skip ':' at start
  if (this->rx_buffer_[0] != HEX_RECORD_START) {
    return;
  }

  // Parse ASCII hex to bytes
  std::vector<uint8_t> decoded;
  for (size_t i = 1; i < this->rx_buffer_.size() - 1; i += 2) {
    if (i + 1 >= this->rx_buffer_.size()) break;

    char hex[3] = {(char)this->rx_buffer_[i], (char)this->rx_buffer_[i + 1], 0};
    uint8_t byte = strtol(hex, nullptr, 16);
    decoded.push_back(byte);
  }

  if (decoded.size() < 2) {
    return;
  }

  uint8_t response_type = decoded[0];

  // Response to GET command (type 7) or SET command (type 8)
  // Response format: TYPE REG_LO REG_HI FLAGS VALUE_LO VALUE_HI CHECKSUM
  if ((response_type == 0x07 || response_type == 0x08) && decoded.size() >= 7) {
    uint16_t reg_id = decoded[1] | (decoded[2] << 8);

    if (reg_id == REG_CHARGE_CURRENT_LIMIT) {
      uint16_t raw_value = decoded[4] | (decoded[5] << 8);
      float value = raw_value / 10.0f;

      ESP_LOGI(TAG, "Received charge current limit: %.1f A (raw: %d)", value, raw_value);

      this->current_value_ = value;
      this->publish_state(value);
      this->waiting_for_response_ = false;
    }
  }
  // Error response (type 0xA = NACK)
  else if (response_type == 0x0A) {
    ESP_LOGW(TAG, "Received NACK response");
    this->waiting_for_response_ = false;
  }
}

}  // namespace victron_charge_limit
}  // namespace esphome
