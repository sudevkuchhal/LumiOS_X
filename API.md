# LumiOS X v1.0 — HTTP API Reference

Base URL: `http://<device-ip>/`. All routes are `GET` except
`POST /update`. Routes marked 🔒 require HTTP Basic Auth when
`settings.authEnabled` is on (default: off).

## Core

| Route | Description |
|---|---|
| `GET /ping` | Liveness check — returns `PONG` |
| `GET /info` | Full status snapshot: WiFi, ESP32, sensors, LEDs, OLED page, settings, boot/reset info, developer info, notification |
| `GET /api/status` | Trimmed status: project, version, SSID, IP, RSSI |

## Version Manager (v1.0)

`GET /api/version`
```json
{
  "firmware": "1.0.0",
  "buildDate": "Jul 20 2026",
  "buildTime": "14:32:01",
  "chip": "ESP32-D0WDQ6 Rev 1",
  "flashSize": "4 MB",
  "freeFlash": "1234 KB",
  "apiVersion": "1.0",
  "dashboardVersion": "1.0",
  "oledVersion": "1.0"
}
```

## Health Monitor (v1.0)

`GET /api/health`
```json
{
  "freeHeap": 123456,
  "heapUsedPercent": 42,
  "flashUsedPercent": 55,
  "fsUsedPercent": 12,
  "cpuLoadPercent": 8,
  "rssi": -58,
  "wifiStatus": "Connected",
  "mqttStatus": "Connected",
  "sensorStatus": "OK",
  "taskStatus": "Loop:OK WiFi:OK MQTT:OK Sensors:OK",
  "restartCount": 0,
  "bootCount": 14,
  "uptime": "00:12:41",
  "healthy": true
}
```
See `ARCHITECTURE.md` for what `cpuLoadPercent` and `taskStatus`
actually measure (both are documented proxies, not FreeRTOS-native
metrics).

## Error / Event Logs (v1.0) 🔒

| Route | Description |
|---|---|
| `GET /api/logs/errors?limit=N` | Newest-first JSON array of error log entries (default 30, capped at `LOG_MAX_LINES`) |
| `GET /api/logs/events?limit=N` | Same, for lifecycle events |
| `GET /api/logs/clear` | Clears both logs |

Error entry shape: `{"t":1234,"cat":"network","msg":"..."}` — `t` is
uptime in seconds (no RTC on this board). `cat` is one of `runtime`,
`network`, `ota`, `sensor`, `storage`, `display`.

Event entry shape: `{"t":1234,"ev":"wifi_connected","detail":"..."}` —
`ev` is one of `wifi_connected`, `wifi_lost`, `mqtt_connected`,
`mqtt_lost`, `ota_started`, `ota_finished`, `notification_created`,
`sensor_added`, `settings_changed`, `boot`, `restart`.

## OTA System (v1.0)

| Route | Description |
|---|---|
| `GET /update` 🔒 | Standalone HTML upload page with its own progress bar |
| `POST /update` 🔒 | Multipart upload, field name `firmware`. Optional query params `?version=X.Y.Z` (reject if not newer than running version) and `&force=1` (bypass that check). Returns `200 text/plain` — `"Update OK. Rebooting..."` or `"Update FAILED: <reason>"` |
| `GET /api/ota/status` | `{"active":false,"progress":0,"status":"Idle","error":"","lastResult":"success","lastOtaUptime":"842","otaEnabled":true}` — poll this while `active` is true for a progress bar |

Rejection reasons you may see in `error`/the POST response: OTA
disabled in Settings, invalid firmware image (bad magic byte),
uploaded file too small, target version not newer than running
version (without `force=1`), flash write failure.

## LED Control 🔒 (all except read-only status in `/info`)

`GET /led/{1-5}/on`, `/off`, `/toggle`, `GET /led/all/on`, `/led/all/off`,
`GET /led/auto/toggle` (LED Automation Engine on/off)

## System / Settings 🔒

| Route | Description |
|---|---|
| `GET /restart` | Safe delayed restart (800ms, lets the response flush) |
| `GET /settings/developer/toggle` | Toggle Developer Mode |
| `GET /settings/animation/toggle` | Toggle OLED animation |
| `GET /settings/autopage/toggle` | Toggle auto page rotation |
| `GET /settings/theme?value=<name>` | Set dashboard theme |
| `GET /settings/auth/toggle` | Toggle Dashboard Auth on/off |
| `GET /settings/auth/credentials?user=&pass=` | Set auth credentials (pass ≥ 8 chars) |

## WiFi 🔒

| Route | Description |
|---|---|
| `GET /wifi/scan` | Scan nearby networks (v1.0: now auth-gated like every other WiFi/LED/OTA control route — previously missing this guard) |
| `GET /wifi/save?ssid=&pass=` | Save credentials and reconnect immediately |

## Notifications

| Route | Description |
|---|---|
| `GET /notifications/history` | JSON array, ring buffer of the last 15 (read-only, not auth-gated) |
| `GET /notifications/clear` 🔒 | Clear notification history |

## Plugin Sensors (Phase 6, unchanged)

| Route | Description |
|---|---|
| `GET /api/plugins` | JSON array of every registered `SensorBase` plugin's full state (read-only, not auth-gated) |
| `GET /api/plugins/toggle?id=<id>` 🔒 | Enable/disable a plugin sensor at runtime |
