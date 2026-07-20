# LumiOS X — M4 Patch: Cloud Relay Reliability Fix

Only 4 files touched: `include/config.h`, `include/cloud_relay.h`,
`src/cloud_relay.cpp`, `data/remote_dashboard.html`. Nothing else in the
project was changed. Topics `MQTT_TOPIC_STATUS` / `MQTT_TOPIC_CMD` and their
values are byte-for-byte unchanged, so anything already relying on them
keeps working.

## Root cause of "dashboard never receives live data"

`cloud_relay.cpp::publishStatus()` spliced `WiFi.localIP()` and the live
notification string directly into a hand-built JSON payload with **no
escaping**. The project already had a `jsonEscape()` helper for this exact
problem in `notification_manager.cpp` — it just was never applied in
`cloud_relay.cpp`. The instant a notification contains a `"` (quite possible
once `/settings/theme` or similar user-editable text feeds a notification),
every subsequent status publish becomes invalid JSON. The dashboard's
`JSON.parse()` was wrapped in a try/catch that silently dropped the message
with no visible error — so the UI looked permanently frozen even though MQTT
traffic was flowing correctly. This is fixed with a local `jsonEscape()` in
`CloudRelay` applied to every dynamic string field.

## Deployment issue (not a code bug, flagging clearly)

`remote_dashboard.html` is only served by the ESP32's own local web server
(`serveStatic("/")`), which is a private LAN address — **not reachable from
mobile data**. The dashboard's JS itself has zero dependency on the ESP32's
IP (by design — it only needs the broker + Device ID), so the fix is where
you *open* the file from, not the code: save it to your phone and open it
locally, or host it on any static web host (GitHub Pages, Netlify, etc). A
note explaining this now appears directly on the page.

## Other fixes / additions (per request)

- **JSON escaping** — `ip` and `notification` fields now pass through
  `CloudRelay::jsonEscape()` before being embedded.
- **Automatic reconnect** — was already present; now uses **exponential
  backoff** (5s → 10s → 20s ... capped at 60s) instead of a fixed 5s retry,
  so a broker outage doesn't spam reconnect attempts.
- **Heartbeat** — new `lumiosx/<id>/heartbeat` topic, published every 5s
  (`MQTT_HEARTBEAT_INTERVAL_MS`), NOT retained. Independent liveness signal.
- **Offline detection** — two layers:
  1. MQTT **Last-Will-Testament** on `lumiosx/<id>/avail` (retained
     "online"/"offline") — the broker publishes "offline" on our behalf if
     the ESP32 drops without a clean disconnect (crash/brownout/WiFi loss).
  2. Dashboard-side watchdog: if no heartbeat *or* status has been seen for
     `OFFLINE_TIMEOUT_MS` (12s), the dot turns amber and the status line
     reads "Device offline — no signal for Ns", even if the browser's own
     broker connection is healthy.
- **Message acknowledgement** — commands are now published as
  `"<id>|<cmd>"` (e.g. `a3f9c1|led1:on`). The ESP32 executes the command and
  publishes `{"id":...,"cmd":...,"ok":true|false}` to `lumiosx/<id>/ack`.
  The dashboard tracks pending acks per button press, times out after 4s if
  none arrives, and logs the result. Payloads with no `|` (bare `"led1:on"`)
  still work exactly as before — fully backward compatible.
- **Retained status** — `/status` is now published with `retain=true`, so a
  phone opening the dashboard mid-session sees the last known state
  immediately instead of waiting up to `MQTT_PUBLISH_INTERVAL_MS` (3s).
- **Structured logging** — firmware side: `[CloudRelay][INFO|WARN|ERROR]`
  lines, plus a human-readable decode of PubSubClient's `rc=` state codes
  (e.g. `-2` now prints as `MQTT_CONNECT_FAILED (couldn't open TCP socket to
  broker)`). Browser side: timestamped `[Dashboard][LEVEL]` console logs for
  every connect/disconnect/publish/ack/parse-failure event.
- **`setKeepAlive(30)` / `setSocketTimeout(5)`** — explicit instead of
  library defaults, so a half-dead TCP socket doesn't stall `loop()`.

## Verified

- `src/cloud_relay.cpp` syntax-checked with `g++ -fsyntax-only -std=c++17`
  against mocks matching the real `Arduino.h` / `WiFi.h` / `PubSubClient.h`
  / project header signatures — compiles clean.
- Embedded `<script>` in `remote_dashboard.html` checked with `node --check`
  — no syntax errors.
- Brace/paren balance verified on all touched files.
- Not run through the real ESP32 toolchain (not available in this
  environment) — please still `pio run` before flashing, same caveat as
  prior changelogs.
