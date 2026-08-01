#include "p180.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace p180 {

static const char *const TAG = "p180";

void P180Component::setup() {}

void P180Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AFERIY / BrightEMS P180 Battery:");
  ESP_LOGCONFIG(TAG, "  Polling interval: %ums", static_cast<unsigned>(this->polling_interval_));
}

void P180Component::loop() {
  if (this->parent_->state() != espbt::ClientState::ESTABLISHED || !this->service_ready_) {
    return;
  }
  const uint32_t now = millis();
  if (now - this->last_poll_ >= this->polling_interval_) {
    this->last_poll_ = now;
    this->send_status_request_();
  }
}

void P180Component::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        memcpy(this->remote_bda_, param->open.remote_bda, sizeof(this->remote_bda_));
        this->conn_id_ = param->open.conn_id;
      }
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *write_char = this->parent_->get_characteristic(
          espbt::ESPBTUUID::from_uint16(P180_SERVICE_UUID_16),
          espbt::ESPBTUUID::from_uint16(P180_WRITE_CHAR_UUID_16));
      auto *notify_char = this->parent_->get_characteristic(
          espbt::ESPBTUUID::from_uint16(P180_SERVICE_UUID_16),
          espbt::ESPBTUUID::from_uint16(P180_NOTIFY_CHAR_UUID_16));

      if (write_char == nullptr || notify_char == nullptr) {
        ESP_LOGW(TAG,
                 "Could not find the expected BrightEMS service (a002) / characteristics "
                 "(c304/c305) on this device. Your P180 may expose different UUIDs - "
                 "connect with nRF Connect to check its actual GATT table.");
        break;
      }

      this->write_handle_ = write_char->handle;
      this->notify_handle_ = notify_char->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->remote_bda_,
                                                        this->notify_handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // Enable notifications on the peer by writing the CCCD (0x2902)
      auto *descr = this->parent_->get_descriptor(
          espbt::ESPBTUUID::from_uint16(P180_SERVICE_UUID_16),
          espbt::ESPBTUUID::from_uint16(P180_NOTIFY_CHAR_UUID_16),
          espbt::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG));
      if (descr != nullptr) {
        uint8_t notify_en[2] = {0x01, 0x00};
        esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->conn_id_, descr->handle,
                                        sizeof(notify_en), notify_en, ESP_GATT_WRITE_TYPE_RSP,
                                        ESP_GATT_AUTH_REQ_NONE);
      }
      this->service_ready_ = true;
      this->last_poll_ = 0;  // poll right away instead of waiting a full interval
      ESP_LOGI(TAG, "Notifications enabled, starting polling");
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->notify_handle_) {
        this->parse_status_response_(param->notify.value, param->notify.value_len);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      this->service_ready_ = false;
      this->write_handle_ = 0;
      this->notify_handle_ = 0;
      if (this->connected_binary_sensor_ != nullptr)
        this->connected_binary_sensor_->publish_state(false);
      break;
    }

    default:
      break;
  }
}

void P180Component::send_status_request_() {
  // Modbus RTU: 11 04 00 00 00 64 [CRC_hi] [CRC_lo]
  // Address 0x11, function 0x04 (read input registers), start reg 0, count 0x64 (100)
  // The P180 natively pushes 100 registers (208-byte frame) - confirmed from its
  // own auto-sent initial notification, unlike the P310's fixed 80/168-byte frame.
  uint8_t cmd[8] = {0x11, 0x04, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00};
  uint16_t crc = crc16_modbus_(cmd, 6);
  cmd[6] = (crc >> 8) & 0xFF;
  cmd[7] = crc & 0xFF;

  esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->conn_id_, this->write_handle_, sizeof(cmd), cmd,
                            ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
}

uint16_t P180Component::crc16_modbus_(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void P180Component::parse_status_response_(const uint8_t *data, uint16_t len) {
  // Enable VERBOSE logging on the "p180" tag to see every raw frame - useful if the
  // register offsets below turn out to be wrong for your specific P180 firmware.
  if (len > 0) {
    char hex[3 * 168 + 1] = {0};
    uint16_t dump_len = len < 168 ? len : 168;
    for (uint16_t i = 0; i < dump_len; i++) {
      snprintf(hex + i * 3, 4, "%02X ", data[i]);
    }
    ESP_LOGV(TAG, "Raw notify (%u bytes): %s", len, hex);
  }

  // Expect: 6-byte header echo + 100 registers (200 bytes) + 2-byte CRC = 208 bytes
  if (len < 208) {
    ESP_LOGD(TAG, "Ignoring short notification (%u bytes, expected 208)", len);
    return;
  }
  if (data[1] != 0x04) {
    // Not a status (input register) response - ignore (could be a 0x03 settings reply)
    return;
  }

  uint16_t crc_calc = crc16_modbus_(data, 206);
  uint16_t crc_recv = (static_cast<uint16_t>(data[206]) << 8) | data[207];
  if (crc_calc != crc_recv) {
    ESP_LOGW(TAG, "CRC mismatch on status frame (calc=%04X recv=%04X), discarding", crc_calc, crc_recv);
    return;
  }

  auto reg = [data](uint16_t n) -> uint16_t {
    uint16_t offset = 6 + (n * 2);
    return (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
  };

  // Register map confirmed by diffing an AC-connected dump against an on-battery
  // dump on a real P180 (Aug 2026). Different offsets than the P310's map.
  if (this->ac_in_voltage_sensor_ != nullptr)
    this->ac_in_voltage_sensor_->publish_state(reg(8) * 0.1f);
  if (this->ac_in_frequency_sensor_ != nullptr)
    this->ac_in_frequency_sensor_->publish_state(reg(9) * 0.01f);
  if (this->ac_out_voltage_sensor_ != nullptr)
    this->ac_out_voltage_sensor_->publish_state(reg(10) * 0.1f);
  if (this->ac_out_frequency_sensor_ != nullptr)
    this->ac_out_frequency_sensor_->publish_state(reg(11) * 0.1f);
  if (this->output_power_sensor_ != nullptr)
    this->output_power_sensor_->publish_state(reg(12));
  if (this->battery_discharge_power_sensor_ != nullptr)
    this->battery_discharge_power_sensor_->publish_state(reg(13));
  if (this->battery_percent_sensor_ != nullptr)
    this->battery_percent_sensor_->publish_state(reg(31));  // raw value IS the percent, no scaling

  // "Remaining time" isn't transmitted as a raw register on this device (confirmed:
  // reg 75 stayed fixed at 144 across multiple real captures while the app's own
  // estimate moved). The app almost certainly computes it client-side the same way
  // we do here: (battery energy remaining) / (current discharge rate).
  if (this->remaining_time_sensor_ != nullptr) {
    float discharge_w = reg(13);
    if (discharge_w > 0.0f) {
      float battery_pct = reg(31);
      float minutes = (battery_pct / 100.0f * this->battery_capacity_wh_ * this->battery_efficiency_) /
                       discharge_w * 60.0f;
      this->remaining_time_sensor_->publish_state(minutes);
    } else {
      // Not discharging (on AC passthrough, or idle) - "remaining time on battery"
      // isn't a meaningful number right now.
      this->remaining_time_sensor_->publish_state(NAN);
    }
  }

  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(true);

  // Grid/AC input presence (the outage sensor): confirmed by direct test - AC input
  // frequency (reg 9) reads 60.00Hz on grid power and drops to exactly 0.00Hz on
  // battery. A small threshold (10Hz) guards against a single noisy sample flapping
  // the sensor right at the boundary.
  if (this->grid_power_binary_sensor_ != nullptr)
    this->grid_power_binary_sensor_->publish_state(reg(9) > 1000);
}

}  // namespace p180
}  // namespace esphome
