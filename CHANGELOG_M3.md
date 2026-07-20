# LumiOS X — M3 Patch Changelog

This patch adds the two concrete gaps still open after M2, cross-checked
against the pasted roadmap/analysis docs against the *actual* current
codebase (a lot of what those docs flagged as missing — sensors, OTA,
notification history, auth — was already built in M2; see
`CHANGELOG_M2.md`). Nothing existing was removed or restructured —
this is additive only.

## ✅ What changed

### LED Automation Engine (Phase 2)
- `leds.h`/`leds.cpp`: each LED now has an automatic role, matching the
  dashboard's existing labels:
  - **LED1 "Power"** → heartbeat pulse (1s) while firmware is alive
  - **LED2 "WiFi"** → solid when connected, blinking (400ms) while not
  - **LED3 "Sensor"** → off when normal, blinking (250ms) on warning,
    solid on danger (smoke/gas or distance threshold)
  - **LED4 "System"** → 2s pulse whenever a new notification fires
  - **LED5 "Alert"** → solid whenever *anything* needs attention (sensor
    warning/danger, or a WARNING/CRITICAL-priority notification)
- Fully non-blocking — driven from `millis()` timers inside
  `LEDManager::updateAuto()`, called once per `loop()`. No `delay()`
  calls were added anywhere in the hot path.
- **Automatic → Manual Override → Automatic Resume**: calling `on()`,
  `off()`, or `toggle()` on any LED while auto mode is running hands
  that *one* LED back to manual control for 30 seconds
  (`AUTO_RESUME_MS`), after which it quietly rejoins automation. The
  other 4 LEDs are unaffected.
- New route: `GET /led/auto/toggle` — flips the whole engine on/off.
- `/info` now includes `ledAutoMode` ("Auto"/"Manual") and
  `ledOverride1..5` (bool per LED).
- Dashboard: new "TOGGLE AUTO MODE" button + mode chip on the LED
  Control Matrix card; LEDs currently under manual override get a
  small "(manual)" label.
- **Existing behavior preserved**: `on()`/`off()`/`toggle()`/`allOn()`/
  `allOff()`/`state()` all still work exactly as before if you never
  call `setAutoMode()` — the engine defaults to *enabled*, but every
  manual control path you had is untouched.

### Notification Priority System (Phase 5)
- `notification_manager.h`/`.cpp`: added `NotifPriority` (`INFO` /
  `WARNING` / `CRITICAL`).
- `setNotification(String message, NotifPriority priority = INFO)` —
  the priority parameter defaults to `INFO`, so every existing call
  site (`webserver.cpp`'s ~10 `setNotification(...)` calls) keeps
  compiling and behaving identically without modification.
- Sensor alert notifications in `main.cpp` now carry real priority:
  smoke danger → `CRITICAL`, smoke/distance warnings → `WARNING`.
  Restart and OTA-flash notifications also marked `WARNING`.
- `/notifications/history` JSON entries now include a `"priority"`
  field; `/info` adds `notifPriority` for the live banner.
- Dashboard: notification list items are now color-coded — amber left
  border for warnings, red for critical (with a 🟠/🔴 badge) — both in
  the live toast feed and the persisted history panel.
- LED5's "Master Alert" role reads this priority directly, so a
  CRITICAL notification lights LED5 solid even if no sensor threshold
  is currently crossed.

## ⚠️ Not done in this pass

- **Not built with the real ESP32 toolchain** — PlatformIO's package
  registry isn't reachable from this sandbox, so an actual
  `pio run` was still not possible. What *was* done this pass, for real:
  brace/paren balance checked on every touched file; `leds.cpp` and
  `notification_manager.cpp` (the two files with logic changes) were
  compiled with `g++ -fsyntax-only` against minimal mocks of
  `Arduino.h`, `WifiManager`, and `SensorManager` matching their real
  method signatures — this caught real type/signature errors, not just
  brace-matching, and came back clean; the dashboard's embedded JS was
  validated with `node --check`. This is a genuine step up from "hand
  read the diff," but it is not a substitute for `pio run` on real
  hardware headers — please still build before flashing.
- True internet weather API (Phase 4's "Weather Module" ask) — the
  existing `PAGE_WEATHER`/`drawWeatherPage()` still shows local
  DHT temperature/humidity, not an internet forecast. That needs an
  API key and `HTTPClient`/`ArduinoJson`, which I didn't want to bolt
  on with a placeholder key; happy to add if you share which weather
  API/key you want to use.
- OTA rollback, dashboard drag-and-drop widgets, AI/anomaly-detection
  features, and the README/PCB/photos release polish (Phases 6, 8–10 in
  the roadmap doc) — out of scope for this pass, not started.

## New/changed endpoints

| Route | Method | Notes |
|---|---|---|
| `/led/auto/toggle` | GET | Flips the LED Automation Engine on/off |

## M3.1 — follow-up (I2C clock fix)

- `display.cpp`: added `Wire.begin(); Wire.setClock(400000);` before
  `display.begin()` in `initDisplay()`. Bumps I2C from the 100kHz
  default to 400kHz fast-mode — genuinely reduces per-frame
  `sendBuffer()` time on the SH1106. `Wire.h` was already pulled in
  transitively via `display.h` → `U8g2lib.h`.
- **Not changed, despite an external review suggesting it:** the boot
  splash font (`u8g2_font_logisoso20_tr`). It's only used once for the
  "LumiOS" logo text for 2 seconds at boot — every real UI page already
  uses `u8g2_font_ncenB08_tr` / `u8g2_font_6x12_tr`. Treating the splash
  font as a live rendering bug was a misread of the code.
- **Not changed:** splitting `data/index.html` into separate
  css/js/html files. That's an architecture trade-off, not a bug —
  flagging for your call rather than doing it silently.

- **Boot splash font changed** (per your call): `u8g2_font_logisoso20_tr`
  → `u8g2_font_ncenB14_tr`, a smaller bold cleaner face, matching the
  weight/style of the header font used everywhere else in the UI
  (`ncenB08_tr`). Repositioned `drawStr(30, 24, "LumiOS")` since the
  cap-height dropped from ~20px to ~14px — the old x/y was tuned for
  the bigger glyph and would've looked off-center with the new one.

| Field | Meaning |
|---|---|
| `ledAutoMode` | `"Auto"` or `"Manual"` |
| `ledOverride1..5` | `true` while that LED is under a 30s manual override |
| `notifPriority` | `"info"` / `"warning"` / `"critical"` for the current notification |

## Stage I — Cloud Relay (remote access from any network)

**Problem:** the dashboard only worked on the same WiFi/local network
as the ESP32 (`http://192.168.x.x`). Off that network — mobile data,
a different WiFi, college/office network — nothing could reach it,
because the ESP32 never had a public/internet-routable address and no
port was forwarded on the router.

**What was added (new files, nothing existing removed):**

- `include/cloud_relay.h`, `src/cloud_relay.cpp` — new `CloudRelay`
  module. Makes an **outbound** connection to a public MQTT broker
  (`broker.hivemq.com:1883`), publishes a compact live-status JSON
  every `MQTT_PUBLISH_INTERVAL_MS` (default 3s) to
  `lumiosx/<IOT_DEVICE_ID>/status`, and subscribes to
  `lumiosx/<IOT_DEVICE_ID>/cmd` for LED commands
  (`led1:on`, `led:all:off`, etc.). Fully non-blocking — same pattern
  as `wifi_manager.cpp`, no `delay()`, self-heals on disconnect.
- `data/remote_dashboard.html` — standalone phone-facing dashboard.
  Connects to the same broker over **MQTT-over-WebSockets**
  (`wss://broker.hivemq.com:8884/mqtt`) using the `mqtt.js` CDN
  library. Doesn't need to be on the ESP32's WiFi or even know its IP
  — it only needs internet and the matching `DEVICE_ID`.
- `include/config.h` — new block: `MQTT_BROKER`, `MQTT_PORT`,
  `IOT_DEVICE_ID`, `MQTT_TOPIC_STATUS`, `MQTT_TOPIC_CMD`,
  `MQTT_PUBLISH_INTERVAL_MS`, `MQTT_RECONNECT_INTERVAL_MS`.
- `platformio.ini` — added `knolleary/PubSubClient@^2.8`.
- `main.cpp` — added `cloudRelay.begin()` in `setup()` (after
  `webServer.begin()`) and `cloudRelay.update()` in `loop()` (next to
  `wifiManager.update()`). Two lines added, nothing else touched.

**Architecture:**

```
Phone (any network)  --wss-->  broker.hivemq.com  <--mqtt-- ESP32 (home WiFi)
```

Both sides only ever dial *out*. No inbound port ever opens on the
router, no VPS, no Cloudflare Tunnel needed — that was the earlier
(wrong) idea, since `cloudflared` needs a full OS and can't run on an
ESP32 at all.

**Before relying on this for anything real:**

- `broker.hivemq.com` is a free/public test broker — no login, no
  encryption on port 1883. `IOT_DEVICE_ID` is your only privacy
  boundary; the placeholder value in `config.h` MUST be changed to
  something unique to you, and the same string must be copied into
  `remote_dashboard.html`'s `DEVICE_ID` constant.
- For anything beyond learning/testing, move to a private broker
  (e.g. HiveMQ Cloud's free tier: TLS + username/password) by changing
  `MQTT_BROKER`/`MQTT_PORT` and adding credentials to `mqtt.connect()`
  in `cloud_relay.cpp` — the rest of the code doesn't change.
- The existing local dashboard (`data/index.html`, port 80) is
  untouched — it still works exactly as before when you're on the
  same WiFi. Cloud Relay is a fully additive second channel, not a
  replacement.

**Not done (flagging, not deciding for you):**
- No command acknowledgement/retry if a command is published while
  the ESP32 is briefly reconnecting to the broker — it'll just be
  missed. Fine for a hobby LED toggle, worth revisiting if commands
  ever get safety-critical.
- `remote_dashboard.html` currently hardcodes the device ID in the
  page; a query-string (`?id=...`) would be nicer if you ever manage
  more than one board.

## Stage I.1 — Login gate on remote_dashboard.html

- Added a username/password lock screen in front of the remote
  dashboard's data view. Credentials (`REMOTE_USER` / `REMOTE_PASS`)
  are set as constants near the top of the `<script>` block in
  `remote_dashboard.html` — change them before sharing the page.
- This is a **client-side** gate (keeps casual/curious people out of
  the page) — it does not encrypt MQTT traffic itself, since
  broker.hivemq.com is still the public test broker. Real security
  still means moving to a private broker with server-side auth (see
  Stage I notes above) — this login screen is a stopgap, not a
  substitute for that.
- Session stays unlocked via `sessionStorage` until the browser tab
  is closed, so it doesn't ask for the password on every refresh.
