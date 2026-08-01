# esphome-p180
Code for using ESPHome BLE to connect to an Afeiry P180 battery for monitoring.

# P180 ESPHome Component (custom, monitoring-focused)

Implements the BrightEMS Modbus-over-BLE protocol documented in
`Ylianst/ESP-FBot`'s `internals/README.md` (reverse-engineered from an
AFERIY P310 teardown). The P180 shares the exact same BLE UUIDs and
Modbus/CRC framing as the P310, **but uses a different, larger register
table (100 registers vs. 80)**. The register offsets below were confirmed
by capturing real data from a live P180 twice — once running on grid
power, once running on battery — and diffing which registers changed:

| Register | AC-connected | On battery | Field |
|---|---|---|---|
| 8  | 1204 (120.4V) | 20 (2.0V)   | AC input voltage |
| 9  | 6000 (60.00Hz) | 0 (0.00Hz) | AC input frequency — **outage sensor** |
| 10 | 1182 (118.2V) | 1189 (118.9V) | AC output voltage (stable) |
| 11 | 600 (60.0Hz)  | 600 (60.0Hz)  | AC output frequency (stable) |
| 12 | 250 | 256 | Output power (W) |
| 13 | 0   | 256 | Battery discharge power (W) |
| 31 | 90  | 90  | Battery % (raw value = %, no scaling) |
| 75 | 144 | 144 | Remaining time (minutes) |

Registers 36/37 (constant `0x3000`/`0x4000` in both captures), 72, 90, and
97–99 (likely firmware/hardware version info) also appeared but their
meaning wasn't needed for outage monitoring, so they're not wired up.
Output on/off state (USB/DC/AC/light) isn't included since the candidate
status register never changed in testing — add it later if you want it,
by capturing dumps with those outputs toggled and diffing the same way.

**This has not been compiled and flashed yet.** The protocol and register
values above are verified against real captured data, but ESPHome's
internal C++ API (exact enum/method names) can vary slightly by version,
so a first compile may need small fixes. If it doesn't compile, paste me
the error and we'll sort it out together.

## Installation

1. Copy the `p180/` folder into your ESPHome config directory, e.g.
   `<config>/esphome/components/p180/`
2. Reference it as a local external component (see example below)
3. Get your battery's BLE MAC address (search for a device starting with
   "FOSSIBOT" or "POWER" using a BLE scanner app or nRF Connect)

## Example configuration

```yaml
esphome:
  name: p180battery
  friendly_name: P180 Battery

esp32:
  board: esp32dev
  framework:
    type: esp-idf

api:
  encryption:
    key: "your_api_encryption_key_here"

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
  level: WARN
  logs:
    p180: VERBOSE   # turn this up if sensors aren't populating - see Troubleshooting

external_components:
  - source:
      type: local
      path: components   # folder containing p180/, relative to this yaml
    components: [p180]

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"   # your P180's BLE MAC
    id: p180_ble

p180:
  id: p180_main
  ble_client_id: p180_ble
  polling_interval: 5s

sensor:
  - platform: p180
    p180_id: p180_main
    ac_in_voltage:
      name: "AC Input Voltage"
    ac_in_frequency:
      name: "AC Input Frequency"
    battery_percent:
      name: "Battery"
    output_power:
      name: "Output Power"
    battery_discharge_power:
      name: "Battery Discharge Power"
    remaining_time:
      name: "Remaining Minutes"

binary_sensor:
  - platform: p180
    p180_id: p180_main
    connected:
      name: "P180 Connected"
    grid_power:
      name: "Grid Power"        # <-- this is your outage sensor
```

In Home Assistant, `binary_sensor.grid_power` going `off` means the P180
detected loss of AC input (a real outage) and switched to battery — that's
the automation trigger you want for alerts/notifications.

## Troubleshooting

**Nothing connects / no sensors populate:**
Set `logger: logs: p180: VERBOSE` as shown above and watch the ESPHome
logs. You should see a `Raw notify (168 bytes): ...` line every polling
interval. If you see nothing at all, the service/characteristic UUIDs
(`a002`/`c304`/`c305`) likely don't match your P180 — confirm with nRF
Connect what it actually exposes and we'll update `p180.h`.

**It connects and logs raw bytes, but values look wrong (e.g. battery %
reads 500% or power spikes to absurd numbers):**
The register *offsets* may differ on the P180's firmware even if the
transport protocol is identical. Paste me a raw hex dump from the log
alongside what the AFERIY app shows at that same moment (battery %,
AC input watts, etc.) and we can work out the correct offsets together.

**CRC mismatch warnings in the log:**
Means frames are arriving but getting corrupted or truncated (often an
MTU issue). Try adding a `mtu:` negotiation step or check that your ESP32
requests a larger MTU on connect — the P310 uses 517 bytes per the docs.

## What's implemented vs. not

- ✅ Battery %, AC input/output voltage & frequency, output power,
  battery discharge power, remaining time, connection state
- ✅ Derived grid-power (outage) binary sensor — confirmed by direct test
- ❌ USB/DC/AC/light on/off status — candidate register didn't change in
  testing, needs another capture with outputs toggled to confirm bit layout
- ❌ Output control (turning USB/DC/AC/light on/off) — not included,
  but straightforward to add via function code `0x06` writes if wanted
- ❌ Settings registers (charge/discharge thresholds, AC charge limit)
  — same story, easy to add on request
