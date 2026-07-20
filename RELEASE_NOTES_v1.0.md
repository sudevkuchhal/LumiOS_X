# LumiOS X — Release Notes: v1.0 Stable

**Release date:** July 2026
**Base:** Sensor Plugin Architecture milestone (v0.94) — unchanged
**Type:** Software feature-complete release. See `CHANGELOG_V1.0.md`
for the full per-phase breakdown and `ARCHITECTURE.md` for design
notes and known limitations.

## Highlights

- 🔄 **OTA firmware updates** — drag-and-drop from the dashboard, with
  validation, progress bar, and OLED status
- 🏷 **Version Manager** — always know exactly what's running
- 💓 **Health Monitor** — heap/flash/storage/CPU/network at a glance,
  with automatic alerts
- 📜 **Error & Event Logs** — persisted, timestamped, queryable via API
- 📶 **WiFi Recovery** — smarter exponential-backoff reconnects
- ⚡ **Dashboard & OLED polish** — lighter blur, better fonts, two new
  screens

## Upgrading from v0.94 (plugin architecture)

This is an additive release — no existing API route, sensor, or
plugin interface changed. If you're running custom plugin sensors
(`REGISTER_SENSOR`), they continue to work unmodified.

1. Re-flash via USB once (OTA can't upgrade the partition table if
   your current build predates `board_build.partitions` being
   explicit — a clean USB flash guarantees the right layout).
2. Re-upload `data/` to LittleFS (`pio run -t uploadfs`) — the
   dashboard has new cards and needs the updated `index.html`.
3. After that, future updates can go over OTA via the dashboard's new
   Firmware Update card.

## Known limitations (be aware before relying on these in the field)

- No OTA rollback — a bad firmware flash does not auto-revert.
- CPU load and "task status" in the Health Monitor are documented
  proxies, not literal FreeRTOS runtime stats.
- No RTC/NTP — log timestamps are uptime-based, not wall-clock.
- WiFi/MQTT backoff timing hasn't been validated against a real
  long-running flaky network.

Full detail in `ARCHITECTURE.md` under "Honest limitations".

## Thanks

Built on top of Stage A–I and the Phase 6 Plugin Architecture work
already in this codebase — v1.0 mostly connects new instrumentation
(health, version, logging, OTA safety) to systems that were already
solid (Cloud Relay's heartbeat/backoff/retained-messages, the
Settings persistence layer, the non-blocking WiFi state machine).
