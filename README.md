# LumiOS X — v1.0 Stable

**An autonomous ESP32 IoT control hub** — sensors, LEDs, an SH1106 OLED,
a live web dashboard, remote cloud access over MQTT, OTA firmware
updates, and a plugin architecture for adding new sensors without
touching existing code.

Board: ESP32 DevKit V1 · Framework: Arduino (PlatformIO) · Filesystem: LittleFS

---

## What's in v1.0 Stable

v1.0 is the "software feature complete" milestone. On top of the
existing Sensor Plugin Architecture (unchanged — see below), this
release adds:

| Area | What it does |
|---|---|
| **OTA System** | In-dashboard drag-and-drop firmware upload with a live progress bar, a standalone `/update` page, firmware image validation, optional version-check gating, automatic reboot, and OLED progress screen. |
| **Version Manager** | Reports firmware/API/dashboard/OLED versions, build date/time, chip model, flash size, and free flash — `GET /api/version`. |
| **Health Monitor** | Heap, flash, LittleFS, a CPU-load estimate, WiFi RSSI, MQTT/WiFi/sensor/task status, boot count, and unexpected-restart count — `GET /api/health`, plus a dedicated OLED HEALTH page and dashboard Health card. |
| **Error / Event Logger** | Persisted, timestamped, categorized logs for runtime/network/OTA/sensor/storage/display errors and for lifecycle events (WiFi/MQTT connect-lost, OTA start/finish, settings changes, boots) — `GET /api/logs/errors`, `GET /api/logs/events`. |
| **Configuration Manager** | Every setting (auth, theme, brightness, OTA toggle, developer mode, etc.) is persisted to LittleFS and reloaded on boot — this already existed as `SettingsManager` + `StorageManager` and now also emits a `settings_changed` event on every save. |
| **WiFi Recovery** | Non-blocking reconnect state machine with exponential backoff (6s → 60s cap) instead of a fixed retry interval. |
| **MQTT / Cloud Relay** | Heartbeat, auto-reconnect with backoff, retained status/availability messages, and offline detection were already implemented (Stage I.2) — v1.0 adds a reconnect-attempt counter and connect/lost event logging on top. |
| **Dashboard Polish** | Reduced backdrop blur (22px/20px → 8px) for performance, better font stack + antialiasing, new Version/Health/OTA cards. |
| **OLED Polish** | New OTA progress screen and HEALTH page, integrated into the existing page-rotation and alert-overlay system without touching any existing page's code. |
| **Documentation** | This README, `ARCHITECTURE.md`, `API.md`, `CHANGELOG_V1.0.md`, `RELEASE_NOTES_v1.0.md`. |

See `ARCHITECTURE.md` for module-by-module detail and honest
limitations (things marked **Hardware Validation Required**).

---

## Sensor Plugin Architecture — unchanged

The plugin system (`sensor_base.h`, `sensor_registry.h/.cpp`) from the
previous milestone is untouched in this release, as requested. Adding
a new sensor is still:

1. Create `include/MySensor.h` — subclass `SensorBase`
2. Create `src/MySensor.cpp` — implement it
3. Add `REGISTER_SENSOR(MySensor)` at the bottom of the `.cpp`

It then automatically gets `begin()`/`update()` calls, a JSON entry on
`GET /api/plugins`, an auto-rendered dashboard card, an OLED PLUGINS
page line, and edge-triggered notifications.

---

## Hardware

| Component | Pin(s) |
|---|---|
| SH1106 OLED (I2C) | SDA 21, SCL 22 |
| LED 1–5 | 13, 14, 25, 26, 27 |
| Onboard status LED | 2 |
| DHT11 | 4 |
| HC-SR04 (Trig / Echo) | 32 / 33 |
| MQ-2 (analog) | 35 |

## Building

```bash
pio run                # build
pio run -t upload      # flash over USB
pio run -t uploadfs    # upload data/ (dashboard HTML) to LittleFS
pio device monitor      # serial monitor, 115200 baud
```

First-time setup: copy `include/wifi_config.example.h` to
`include/wifi_config.h` and fill in your fallback WiFi credentials.
Set a unique `IOT_DEVICE_ID` in `include/config.h` before exposing
Cloud Relay to the public broker — see the comment block above it.

## Folder Tree

```
LumiOS_X_v1.0_Stable/
├── include/                 # headers — one per module
│   ├── config.h             # all pins, thresholds, topics, versions
│   ├── sensor_base.h        # plugin sensor interface (unchanged)
│   ├── sensor_registry.h    # plugin registry/self-registration (unchanged)
│   ├── version_manager.h    # v1.0
│   ├── health_monitor.h     # v1.0
│   ├── error_logger.h       # v1.0
│   ├── event_logger.h       # v1.0
│   ├── ota_manager.h        # v1.0
│   └── ...                  # display, wifi, cloud_relay, settings, etc.
├── src/                      # implementation, mirrors include/
├── data/                      # LittleFS contents — dashboards
│   ├── index.html            # local dashboard (same-network)
│   └── remote_dashboard.html # Cloud Relay dashboard (any network)
├── platformio.ini
├── README.md                 # this file
├── ARCHITECTURE.md
├── API.md
├── CHANGELOG_V1.0.md
└── RELEASE_NOTES_v1.0.md
```

## Web API quick reference

See `API.md` for the full list. Highlights new in v1.0:

```
GET  /api/version         -> firmware/build/chip info
GET  /api/health          -> heap/flash/fs/cpu/wifi/mqtt/sensor status
GET  /api/logs/errors     -> recent error log (auth required)
GET  /api/logs/events     -> recent event log (auth required)
GET  /api/ota/status      -> current/last OTA state
GET  /update               -> standalone OTA upload page
POST /update               -> firmware upload (multipart, field "firmware")
```

## License / Author

Built by Sudev as a personal IoT/embedded systems project. No license
file included — treat as private/personal project code unless the
author states otherwise.
