# LumiOS X v1.0 — Architecture

## Design philosophy

Every module in this codebase follows the same rule established in
earlier milestones: **additive, non-blocking, single-responsibility**.
Nothing calls `delay()` outside of the boot screen. Every manager
class exposes `begin()`/`update()` and is driven from `main.cpp`'s
`loop()`. New functionality is added as new files with new
`begin()`/`update()` hooks wired into `main.cpp`, not by editing
existing modules' internals — this is why the Sensor Plugin
Architecture, Cloud Relay, and all Stage A–I work from before v1.0
did not need to change to add the v1.0 feature set.

## Module map (v1.0 additions)

```
main.cpp
 ├─ StorageManager        (LittleFS key=value persistence — unchanged)
 ├─ ErrorLogger    [NEW]  persisted categorized error log
 ├─ EventLogger    [NEW]  persisted lifecycle event log
 ├─ VersionManager [NEW]  build/chip/flash reporting (read-only)
 ├─ HealthMonitor  [NEW]  polls other modules, fires threshold alerts
 ├─ OTAManager     [NEW]  wraps Update.h with validation + progress
 ├─ WiFiManagerX           (+ exponential backoff, event logging)
 ├─ CloudRelay             (unchanged core; + status getters, event logging)
 ├─ SettingsManager         (unchanged; save() now also logs an event)
 ├─ SensorManager / SensorRegistry   (unchanged — Plugin Architecture)
 ├─ DisplayManager          (+ PAGE_HEALTH, + OTA progress screen)
 └─ WebServerX              (+ /api/version, /api/health, /api/logs/*,
                               /api/ota/status; /update upgraded to
                               route through OTAManager)
```

### Data flow for a typical health alert

```
HealthMonitor::update() [every 4s]
  -> reads ESP.getFreeHeap(), LittleFS.usedBytes(), etc.
  -> crosses a threshold in config.h (e.g. HEALTH_HEAP_CRIT_BYTES)
  -> NotificationManager::setNotification(msg, CRITICAL)
       -> bumps notificationSeq
       -> OLED: updateDisplay() sees the new seq, shows drawAlertOverlay()
       -> Dashboard: next /info or /api/status poll shows the notification
       -> Cloud Relay: next publishStatus() includes it (MQTT, retained)
```

No new OLED/dashboard code was needed for alerts themselves — Health
Monitor reuses the existing NotificationManager pipeline built for
sensor alerts.

### Data flow for an OTA update

```
Dashboard OTA card (or /update page)
  -> POST /update (multipart, field "firmware")
     -> webserver.cpp upload handler, first chunk:
          otaManager.onUploadStart()  — checks settings.otaEnabled,
                                          calls Update.begin()
     -> each chunk:
          otaManager.onUploadWrite()  — validates magic byte on the
                                          very first chunk, then
                                          Update.write()
     -> final chunk:
          Update.end(true) -> otaManager.onUploadEnd()
                                — persists result to /otainfo.txt,
                                  logs an event
  -> meanwhile, every loop(): updateDisplay() sees
     otaManager.isActive() and shows showOTAScreen() instead of the
     normal page rotation (OTA priority lock)
  -> on success: systemManager.requestRestart() -> ESP.restart() after
     an 800ms delay (lets the HTTP response flush first)
```

## Honest limitations (flagged inline in code comments too)

These are documented rather than hidden, per the "Hardware Validation
Required" convention:

1. **CPU Load Estimate is a proxy, not a true scheduler metric.**
   `HealthMonitor::recordLoopTiming()` measures how much of each
   `loop()` iteration was spent working vs. the trailing `delay(200)`.
   It is a reasonable "is the main loop busy" indicator but not a
   FreeRTOS-level per-task CPU percentage (that requires
   `configGENERATE_RUN_TIME_STATS` build flags this project doesn't
   currently enable).

2. **"Task Status" is a subsystem heartbeat, not a real task list.**
   The Arduino loop() model here is effectively single-task (plus
   AsyncTCP's own task for the web server). `HealthMonitor::getTaskStatus()`
   reports WiFi/MQTT/Sensor reachability, which is the practically
   useful signal, but isn't literally a FreeRTOS `vTaskList()` dump.

3. **OTA rollback is NOT implemented.** This firmware does not call
   `esp_ota_mark_app_valid_cancel_rollback()` /
   `esp_ota_mark_app_invalid_and_reboot()`. If a bad image is flashed
   and boots into a broken state, it will NOT automatically revert to
   the previous partition. **Hardware Validation Required** before
   relying on this in the field — either add app-rollback support or
   keep a USB cable within reach when testing new firmware.

4. **Version Check on upload is metadata-level, not binary-level.**
   The optional `?version=X.Y.Z` query param on `POST /update` is
   compared against the compiled-in `FIRMWARE_VERSION`, not parsed
   out of the uploaded `.bin`'s embedded `esp_app_desc_t`. Extracting
   the real embedded version from an Arduino-built `.bin` during
   upload would require additional build tooling (e.g. a custom
   `esp_app_desc_t` reader) not currently part of this project.

5. **Partition table assumption.** OTA needs `ota_0`/`ota_1` app
   partitions. `platformio.ini` now explicitly pins
   `board_build.partitions = default.csv`, which provides these on a
   4MB flash chip. **Hardware Validation Required**: confirm your
   physical board's actual flash size before trusting OTA — a 2MB
   board needs a smaller custom partition table.

6. **No RTC / NTP wall clock.** Error/event log timestamps are
   uptime-in-seconds (`millis()/1000`), not real calendar time. This
   is honest given the board has no RTC and NTP sync isn't part of
   this release — adding NTP (`configTime()` + WiFi) would be a
   natural follow-up if wall-clock timestamps matter later.

7. **WiFi/MQTT reconnect timing is a state machine, not something
   validated against real-world flaky-router conditions.**
   **Hardware Validation Required**: the exponential backoff curve
   (6s → 12s → 24s → 48s → 60s cap) is reasonable but untested against
   an actual unstable access point over hours/days.

## Persisted files (LittleFS)

| File | Written by | Purpose |
|---|---|---|
| `/wifi.txt` | StorageManager | last-known-good WiFi credentials |
| `/settings.txt` | StorageManager | all SettingsManager fields |
| `/bootcount.txt` | StorageManager | every boot, including clean restarts |
| `/crashcount.txt` **[NEW]** | StorageManager | only panic/watchdog/brownout resets |
| `/errorlog.txt` **[NEW]** | ErrorLogger | rotating, capped at `LOG_MAX_LINES` |
| `/eventlog.txt` **[NEW]** | EventLogger | rotating, capped at `LOG_MAX_LINES` |
| `/otainfo.txt` **[NEW]** | OTAManager | last OTA result/time/size |
