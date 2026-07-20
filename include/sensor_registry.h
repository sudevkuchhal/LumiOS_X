#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <Arduino.h>
#include "sensor_base.h"

// Future plugin sensor slots. Bump if a project ever needs more than 8 —
// it's a fixed array on purpose (no heap churn from a dynamic container
// on an ESP32 that's already juggling AsyncWebServer + MQTT + U8g2 buffers).
#define SENSOR_REGISTRY_MAX 8

class SensorRegistry
{
public:
    // Meyer's singleton — safe against static-initialization-order
    // issues, which matters here because REGISTER_SENSOR() below calls
    // this from a global constructor, possibly before/after other
    // global objects (wifiManager, settings, etc.) are constructed.
    static SensorRegistry &instance();

    // Called by REGISTER_SENSOR() during static init. Not meant to be
    // called directly from application code.
    bool registerSensor(SensorBase *sensor);

    void beginAll();  // call once from setup(), after sensorManager.begin()
    void updateAll(); // call every loop(); also dispatches edge-triggered alerts

    uint8_t count() const { return sensorCount; }
    SensorBase *get(uint8_t index) const;
    SensorBase *find(const String &id) const;

    // JSON array of every registered plugin sensor's full state.
    // Powers GET /api/plugins and, through it, the dashboard's generic
    // plugin-card renderer. Returns "[]" when nothing is registered.
    String getAllJson() const;

    // Short lines for the auto OLED PLUGINS page.
    void getOledSummaryLines(String lines[], uint8_t maxLines, uint8_t &outCount) const;

private:
    SensorRegistry() {}

    SensorBase *sensors[SENSOR_REGISTRY_MAX] = {nullptr};
    SensorAlertLevel lastAlertLevel[SENSOR_REGISTRY_MAX] = {SensorAlertLevel::NONE};
    uint8_t sensorCount = 0;
};

// Style-consistent alias so call sites read like the rest of the
// codebase's global singletons (wifiManager, settings, leds, ...)
// while actually resolving to the safe Meyer's-singleton instance.
#define sensorRegistry SensorRegistry::instance()

// ==========================================================
// Self-registration — this is what makes "just create the file and
// register it" true. REGISTER_SENSOR(Class) declares a static
// instance of the sensor plus a tiny helper whose constructor runs at
// static-init time and registers it into SensorRegistry. Put this one
// line at the bottom of your new sensor's .cpp file; nothing else
// needs to know the class exists.
// ==========================================================
struct SensorRegistrarHelper
{
    explicit SensorRegistrarHelper(SensorBase *s) { SensorRegistry::instance().registerSensor(s); }
};

#define REGISTER_SENSOR(ClassName)                                        \
    static ClassName ClassName##_instance;                                \
    static SensorRegistrarHelper ClassName##_registrar(&ClassName##_instance)

#endif
