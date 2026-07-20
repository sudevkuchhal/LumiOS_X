#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — Sensor Plugin Architecture (Phase 6)
//
// This is a SEPARATE, ADDITIVE system for FUTURE sensors. It does not
// touch or replace SensorManager (DHT11 / HC-SR04 / MQ2) — that keeps
// working exactly as it does today, unchanged. This exists purely so
// the NEXT sensor doesn't require editing main.cpp / webserver.cpp /
// display.cpp at all.
//
// TO ADD A NEW SENSOR:
//   1. Create include/MySensor.h   — subclass SensorBase below
//   2. Create src/MySensor.cpp     — implement it
//   3. At the bottom of MySensor.cpp, add:  REGISTER_SENSOR(MySensor)
//   Done. From that point on it automatically gets:
//     - begin()/update() called every boot/loop
//     - a JSON entry on GET /api/plugins
//     - a dashboard card (the dashboard's generic plugin renderer
//       reads /api/plugins and builds cards — no HTML/JS edits needed)
//     - a summary line on the OLED PLUGINS page
//     - edge-triggered notifications when getAlertLevel() flips
//       from NONE into WARNING/CRITICAL
//   See sensor_registry.h for the registration macro itself.
// ==========================================================

enum class SensorAlertLevel
{
    NONE,
    WARNING,
    CRITICAL
};

class SensorBase
{
public:
    virtual ~SensorBase() {}

    // Stable, machine-readable id (e.g. "dht22_greenhouse"). Used as the
    // JSON key and as this sensor's settings/logging namespace.
    virtual const char *getId() const = 0;

    // Human-readable name for the dashboard card / OLED line.
    virtual const char *getDisplayName() const = 0;

    // Called once from setup(), after sensorRegistry.beginAll() runs —
    // do pin/library init here, same as SensorManager::begin() does.
    virtual void begin() = 0;

    // Called every loop() (only while isEnabled()). Do your own
    // internal millis() throttling here, same convention already used
    // by SensorManager::update() for the DHT/HC-SR04/MQ2 reads.
    virtual void update() = 0;

    // Current alert state. SensorRegistry edge-detects transitions
    // itself, so you don't need to track "did I already warn about
    // this" — just report the current level truthfully each call.
    virtual SensorAlertLevel getAlertLevel() const { return SensorAlertLevel::NONE; }

    // Short text pushed into NotificationManager (and therefore the
    // dashboard + OLED alert overlay) the moment getAlertLevel()
    // transitions into WARNING or CRITICAL.
    virtual String getAlertMessage() const { return ""; }

    // One short line (<=21 chars fits the OLED 6x12 font width) for the
    // auto-generated PLUGINS OLED page, e.g. "Soil: 42% OK".
    virtual String getOledSummary() const = 0;

    // Full sensor state as JSON object *fields only*, no outer braces —
    // e.g. return "\"value\":23.4,\"unit\":\"C\"". SensorRegistry wraps
    // this with id/name/enabled/alert automatically in getAllJson().
    virtual String getJsonFields() const = 0;

    // Runtime enable/disable — toggled via
    // GET /api/plugins/toggle?id=<id> (requires auth, same as other
    // settings routes). Disabled sensors are skipped by updateAll()
    // and still appear on /api/plugins, just flagged "enabled":false.
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e) { enabled = e; }

private:
    bool enabled = true;
};

#endif
