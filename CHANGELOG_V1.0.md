# CHANGELOG — v1.0 Stable

Continues from `CHANGELOG_M2.md`, `CHANGELOG_M3.md`, and
`CHANGELOG_M4_CLOUD_RELAY_FIX.md`. Base: the Sensor Plugin
Architecture milestone (v0.94), left unchanged in this release.

## [1.0.0] — LumiOS X Stable

### 1. OTA System
- New `OTAManager` (`ota_manager.h/.cpp`) wraps the existing Stage G
  `/update` upload handler with real progress tracking.
- **Firmware Validation**: rejects uploads whose first byte isn't the
  ESP32 app-image magic byte (`0xE9`) before committing anything to
  flash; also rejects files under `OTA_MIN_FIRMWARE_BYTES`.
- **Version Check**: optional `?version=X.Y.Z` query param on the
  upload rejects a same-version reflash unless `&force=1` is present.
- **Protected OTA**: gated by the existing `settings.otaEnabled`
  toggle *and* HTTP Basic Auth (both pre-existing, now both enforced
  at the OTA-manager level too, not just the route level).
- **Automatic Reboot**: unchanged — `systemManager.requestRestart()`
  after a successful flash.
- **Dashboard OTA Status**: new inline drag-and-drop upload card with
  a live progress bar (`GET /api/ota/status`, polled every 2s).
- **OLED OTA Status**: new `showOTAScreen()` — takes over the display
  the moment an upload starts, priority-locked above the normal page
  rotation and the notification overlay.

### 2. Version Manager
- New `VersionManager` (`version_manager.h/.cpp`) and `GET /api/version`.
- Reports firmware/API/dashboard/OLED versions (independently
  versioned via new `config.h` constants), build date/time
  (`__DATE__`/`__TIME__`), ESP32 chip model, flash size, and free
  flash (sketch space headroom).
- New "Version Manager" dashboard card.

### 3. Health Monitor
- New `HealthMonitor` (`health_monitor.h/.cpp`) and `GET /api/health`.
- Heap, flash, and LittleFS usage percentages; a documented CPU-load
  *estimate* (proxy metric — see `ARCHITECTURE.md`); WiFi RSSI; MQTT
  status; WiFi status; sensor status; a subsystem-heartbeat "task
  status"; restart count (new, crash-only, see below) and boot count;
  uptime.
- Threshold crossings (low heap, high LittleFS usage) fire through
  the existing `NotificationManager`, so they automatically appear as
  OLED alert overlays and dashboard notifications — no new alert
  plumbing needed.
- New OLED **HEALTH** page (`PAGE_HEALTH`), appended to the page
  rotation without renumbering any existing page.
- New dashboard "Health Monitor" card with animated usage bars.
- `StorageManager` gained `incrementCrashCount()`/`getCrashCount()` —
  a *separate* counter from the existing boot counter, incremented
  only on panic/watchdog/brownout resets, so "Restart Count" means
  something different from "every single boot".

### 4. Error Logger
- New `ErrorLogger` (`error_logger.h/.cpp`), persisted to
  `/errorlog.txt`, rotating at `LOG_MAX_LINES`.
- Categories: Runtime, Network, OTA, Sensor, Storage, Display.
- Every entry timestamped (uptime seconds — no RTC on this board, see
  `ARCHITECTURE.md`).
- Wired into: WiFi Recovery (all-attempts-failed), Cloud Relay (MQTT
  connect failures), OTA Manager (every validation/write/flash
  failure), unexpected-restart detection in `main.cpp`.
- `GET /api/logs/errors?limit=N` (auth required).

### 5. Event Logger
- New `EventLogger` (`event_logger.h/.cpp`), same persisted/rotating
  design as the Error Logger, `/eventlog.txt`.
- Logs exactly the event set requested: WiFi Connected/Lost, MQTT
  Connected/Lost, OTA Started/Finished, Notification Created (fired
  from the existing sensor-alert path), Sensor Added (fired for
  future plugin-sensor registrations), Settings Changed (fired from
  every `SettingsManager::save()` call), Boot, Restart.
- `GET /api/logs/events?limit=N` (auth required).

### 6. Configuration Manager
- Already largely in place from Stage H (`SettingsManager` +
  `StorageManager::saveSettings/loadSettings`) — every toggle
  (developer mode, auto-page, animation, OTA enable, brightness,
  theme, language, auth) persists to `/settings.txt` and reloads on
  boot.
- v1.0 addition: `SettingsManager::save()` now also emits a
  `settings_changed` event through the Event Logger on every write.

### 7. WiFi Recovery
- `WiFiManagerX` reconnect cooldown changed from a fixed 6s wait to
  **exponential backoff**: 6s → 12s → 24s → 48s → 60s (capped),
  resetting to 6s the moment a connection succeeds.
- New `getReconnectAttempts()` / `getNextRetryMs()` getters; OLED WiFi
  status text now shows "Retrying in Ns" during cooldown instead of a
  flat "Retrying soon".
- WiFi connect/lost transitions now logged through the Event Logger;
  a full failed retry cycle logs through the Error Logger.

### 8. MQTT Improvements
- Heartbeat, auto-reconnect with backoff, retained status/availability
  messages (Last-Will-Testament `/avail` topic), and offline detection
  were **already implemented** in the prior Stage I.2 milestone —
  confirmed present and unchanged.
- v1.0 adds: `CloudRelay::getStateText()` / `getReconnectFailCount()`
  getters (feed the new Health Monitor and could back a dashboard
  "reconnect indicator" widget), and MQTT connect/lost events logged
  through the Event Logger.

### 9. Dashboard Polish
- Backdrop blur reduced from 22px/20px to 8px on the header and cards
  (performance — heavy blur is expensive to composite, especially on
  older phones), with panel background opacity raised slightly to
  compensate visually.
- Font stack upgraded (`Inter` first, with the previous stack as
  fallback) plus `-webkit-font-smoothing: antialiased` and
  `text-rendering: optimizeLegibility`.
- New Version / Health / OTA cards, styled consistently with the
  existing `.card`/`.row`/`.chip` design language (no new visual
  system introduced).

### 10. OLED Polish
- New **HEALTH** page and **OTA progress** screen, added without
  modifying any existing page's drawing code.
- Existing boot animation, page-rotation, and alert-overlay systems
  are untouched — the OTA screen integrates via a priority-lock check
  at the top of `updateDisplay()`, the same pattern already used for
  the notification alert overlay.

### 11. Performance
- Reduced dashboard backdrop-filter cost (see Dashboard Polish).
- Health Monitor and loggers are all throttled/append-only —
  `HealthMonitor::update()` only does work every
  `HEALTH_CHECK_INTERVAL_MS` (4s), not every loop() iteration.
- No changes made to the existing sensor/display/WiFi timing — those
  were already non-blocking and are preserved as-is per the
  "preserve every module" instruction.

### 12. Code Cleanup
- Every new module follows the existing project conventions:
  `begin()`/`update()` lifecycle, `extern` global singleton pattern,
  `String`-based hand-built JSON (matches the rest of the codebase
  rather than introducing a JSON library dependency), `const`-correct
  getters, categorized/commented sections.
- Fixed one small, real bug found while doing this pass: `/wifi/scan`
  was missing the `requireAuth()` guard that every other WiFi/LED/OTA
  control route already has — now consistent.
- No existing function was renamed, removed, or had its signature
  changed; all v1.0 work is additive.

### 13. Documentation
- Added `README.md`, `ARCHITECTURE.md`, `API.md`,
  `CHANGELOG_V1.0.md` (this file), `RELEASE_NOTES_v1.0.md`.
- `platformio.ini` now explicitly documents the OTA partition-table
  assumption (`board_build.partitions = default.csv`, 4MB flash).

---

## Hardware Validation Required

Flagged consistently in code comments and in `ARCHITECTURE.md`:
- OTA rollback (`esp_ota_mark_app_invalid_and_reboot`) is not
  implemented — a bad flash will not auto-revert.
- WiFi/MQTT exponential-backoff timing is implemented but not
  field-tested against a real flaky access point over long periods.
- The 4MB-flash / `default.csv` partition assumption should be
  confirmed against your actual board before relying on OTA.
