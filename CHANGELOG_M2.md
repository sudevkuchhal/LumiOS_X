# LumiOS X — M2 Patch Changelog

This patch closes several of the gaps identified in the original roadmap
review. It focuses on making the **backend real** (the frontend dashboard
was already ~90% built — most of the missing 10-15% was hardcoded
placeholder data and unwired buttons), plus a few structural additions
(OTA, auth, boot tracking).

## ✅ What changed

### Stage C — Sensor Expansion
- `sensor_manager.h/.cpp` rewritten: real **HC-SR04 distance** (`pulseIn`,
  non-blocking, 200ms throttle) and **MQ2 smoke/gas** (`analogRead`) with
  warning/danger thresholds — previously these were hardcoded `0` in
  `/info`.
- New pins/thresholds in `config.h`: `TRIG_PIN` (32), `ECHO_PIN` (33),
  `MQ2_PIN` (35), `DISTANCE_WARN_CM`, `SMOKE_WARN_RAW`, `SMOKE_DANGER_RAW`.
  **Adjust these pins to match your actual wiring before flashing.**
- `main.cpp` now edge-detects threshold crossings and pushes a
  notification once per event (not every 200ms loop).

### Stage E — Notification Console
- `notification_manager.h/.cpp` now keeps a **15-entry ring buffer** with
  timestamps, exposed as `/notifications/history` (JSON array) and
  `/notifications/clear`.
- Dashboard fetches history on load and renders it into the Notification
  Console list; added a "Clear History" button.

### Stage G — System Manager
- `storage_manager` persists a **boot counter** to LittleFS
  (`/bootcount.txt`), incremented once per boot.
- `system_manager` now returns a **human-readable reset reason**
  ("Power-On", "Brownout (Power Dip)", "Crash / Panic", etc.) instead of
  a raw enum int, plus chip core count and revision.
- All of the above, plus developer info (name/role/GitHub/LinkedIn/
  repos/stars/status) and full settings state, are now included in
  `/info` — previously only theme + developer mode were exposed.

### Stage B — Real OLED Mirror
- The OLED "mirror" was previously a static styled `<div>` showing only
  4 fixed lines regardless of which page the physical display was on.
- Replaced with a **128×64 `<canvas>`** that redraws the *actual* content
  of whichever page is active (`HOME`, `WIFI`, `SYSTEM`, `SENSORS`,
  `DEVELOPER`, `NOTIFICATIONS`, `SETTINGS`), line-for-line matching the
  positions/text used in `display.cpp`'s `drawXPage()` functions,
  including the Developer/Settings page sub-rotations.

### Stage G — OTA Firmware Updates
- New `/update` route: `GET` serves a minimal upload page, `POST`
  streams a `.bin` file into `Update.h` and reboots on success.
- No new library needed — `Update.h` is part of the ESP32 Arduino core.

### Stage H — Authentication
- New optional HTTP Basic Auth, **off by default** (so the existing
  dashboard keeps working with zero config).
- `settings.authEnabled` / `authUser` / `authPass`, toggled via
  `/settings/auth/toggle`, credentials changed via
  `/settings/auth/credentials?user=...&pass=...`.
- Guards added to the routes that actually matter: `/restart`,
  `/wifi/save`, `/led/all/on`, `/led/all/off`, `/update`.
- New dashboard buttons: "OTA FIRMWARE UPDATE" and "TOGGLE DASHBOARD
  AUTH", plus live chips showing boot count / last reset reason / auth
  state.

### Dashboard polish
- Distance/smoke rows now color-code live (`good`/`warn`/`bad`) based on
  the new alert flags, plus a combined "Sensor Status" row.

## ⚠️ Not done in this pass (be aware)

- **Not compiled.** No PlatformIO toolchain was available in the sandbox
  this was written in. Run `pio run` locally before flashing — I've
  hand-checked brace/paren balance and header/impl signatures match, but
  a real ESP32 toolchain build is the only true test.
- **Hardware pins are guesses.** `TRIG_PIN`/`ECHO_PIN`/`MQ2_PIN` in
  `config.h` need to match your actual wiring — I picked free-looking
  GPIOs (32/33/35) but you know your board's wiring, I don't.
- Full README/architecture-diagram/screenshots (Stage J) — not written.
- Dashboard mobile-responsiveness pass (Stage I) — not audited; the
  existing CSS already uses `grid`/`flex-wrap` so it's probably decent,
  but not verified on a real small viewport.
- Deeper "developer console" tools (serial log viewer, per-task heap
  monitor) — the roadmap's more exotic asks weren't built; what exists
  now is ping, WiFi scan/save (already existed), restart, and the new
  OTA/auth/history endpoints.

## New/changed endpoints

| Route | Method | Notes |
|---|---|---|
| `/notifications/history` | GET | JSON array, newest first |
| `/notifications/clear` | GET | Clears current + display |
| `/settings/auth/toggle` | GET | Auth-guarded once enabled |
| `/settings/auth/credentials` | GET | `?user=&pass=` (min 4 char pass) |
| `/update` | GET | OTA upload form |
| `/update` | POST | multipart `.bin` upload, flashes + reboots |
