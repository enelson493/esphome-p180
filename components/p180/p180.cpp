#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <esp_gattc_api.h>

namespace esphome {
namespace p180 {

namespace espbt = esphome::esp32_ble_tracker;

// BrightEMS / SYD-power BLE service & characteristics
// Documented in Ylianst/ESP-FBot's internals/README.md (AFERIY P310 teardown).
// If your P180 doesn't respond, these UUIDs are the first thing to double check
// with nRF Connect - the register map is more likely to differ than these.
static const uint16_t P180_SERVICE_UUID_16 = 0xa002;      // 0000a002-0000-1000-8000-00805f9b34fb
static const uint16_t P180_WRITE_CHAR_UUID_16 = 0xc304;   // client -> device
static const uint16_t P180_NOTIFY_CHAR_UUID_16 = 0xc305;  // device -> client

class P180Component : public esphome::ble_client::BLEClientNode, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;

  void set_polling_interval(uint32_t ms) { this->polling_interval_ = ms; }

  // Sensors - register numbers empirically confirmed on a real P180 by diffing an
  // AC-connected dump against an on-battery dump (Aug 2026). NOT the same offsets
  // as the P310 - this device uses a 100-register table with different field order.
  void set_ac_in_voltage_sensor(sensor::Sensor *s) { this->ac_in_voltage_sensor_ = s; }       // reg 8
  void set_ac_in_frequency_sensor(sensor::Sensor *s) { this->ac_in_frequency_sensor_ = s; }   // reg 9
  void set_ac_out_voltage_sensor(sensor::Sensor *s) { this->ac_out_voltage_sensor_ = s; }     // reg 10
  void set_ac_out_frequency_sensor(sensor::Sensor *s) { this->ac_out_frequency_sensor_ = s; } // reg 11
  void set_output_power_sensor(sensor::Sensor *s) { this->output_power_sensor_ = s; }         // reg 12
  void set_battery_discharge_power_sensor(sensor::Sensor *s) { this->battery_discharge_power_sensor_ = s; } // reg 13
  void set_battery_percent_sensor(sensor::Sensor *s) { this->battery_percent_sensor_ = s; }   // reg 31 (raw = %, no scaling)
  void set_remaining_time_sensor(sensor::Sensor *s) { this->remaining_time_sensor_ = s; }     // reg 75

  // Binary sensors
  void set_connected_binary_sensor(binary_sensor::BinarySensor *s) { this->connected_binary_sensor_ = s; }
  // Derived: is grid/AC power actually present at the input (the outage sensor) -
  // confirmed by direct test against real AC-loss on this device.
  void set_grid_power_binary_sensor(binary_sensor::BinarySensor *s) { this->grid_power_binary_sensor_ = s; }
  // NOTE: output-active flags (USB/DC/AC/light on-off) aren't wired up yet - the
  // candidate status registers (36/37) didn't change in either test capture, so
  // their bit layout on this device isn't confirmed. Add later if needed.

 protected:
  void send_status_request_();
  void parse_status_response_(const uint8_t *data, uint16_t len);
  static uint16_t crc16_modbus_(const uint8_t *data, uint16_t len);

  uint32_t polling_interval_{5000};
  uint32_t last_poll_{0};
  bool service_ready_{false};
  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};
  esp_bd_addr_t remote_bda_{};  // captured from ESP_GATTC_OPEN_EVT - parent's copy is protected
  uint16_t conn_id_{0};         // captured from ESP_GATTC_OPEN_EVT - parent's copy is protected

  sensor::Sensor *ac_in_voltage_sensor_{nullptr};
  sensor::Sensor *ac_in_frequency_sensor_{nullptr};
  sensor::Sensor *ac_out_voltage_sensor_{nullptr};
  sensor::Sensor *ac_out_frequency_sensor_{nullptr};
  sensor::Sensor *output_power_sensor_{nullptr};
  sensor::Sensor *battery_discharge_power_sensor_{nullptr};
  sensor::Sensor *battery_percent_sensor_{nullptr};
  sensor::Sensor *remaining_time_sensor_{nullptr};

  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *grid_power_binary_sensor_{nullptr};
};

}  // namespace p180
}  // namespace esphome
