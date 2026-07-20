#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — Health Monitor (v1.0)
//
// Rolls up everything needed for the dashboard Health Card and OLED
// health alerts: heap, flash, LittleFS usage, a rough CPU load
// estimate, WiFi RSSI, MQTT/WiFi/sensor/task status, restart & boot
// counts and uptime. Purely a READER of other modules (SensorManager,
// SensorRegistry, WiFiManagerX, CloudRelay, StorageManager) — it does
// not own any hardware itself, so it can't conflict with anything.
//
// update() is throttled internally (HEALTH_CHECK_INTERVAL_MS) and
// edge-detects threshold crossings so it fires a NotificationManager
// alert once per transition rather than spamming every loop().
// ==========================================================

class HealthMonitor
{
public:
    void begin();
    void update(); // call every loop()

    // Called once per loop from main.cpp so the CPU estimate reflects
    // real work done, not a guess. activeMs = time spent doing work
    // this loop iteration BEFORE the trailing delay(); totalMs =
    // activeMs + that delay.
    void recordLoopTiming(unsigned long activeMs, unsigned long totalMs);

    // ---- Memory / Storage ----
    uint32_t getFreeHeap();
    uint8_t  getHeapUsedPercent();
    uint8_t  getFlashUsedPercent();   // sketch bytes used / total sketch space
    uint8_t  getFsUsedPercent();      // LittleFS used / total

    // ---- CPU ----
    uint8_t getCpuLoadPercent();      // rolling estimate, 0-100

    // ---- Connectivity ----
    int     getRSSI();
    String  getWiFiStatus();
    String  getMqttStatus();
    String  getSensorStatus();
    String  getTaskStatus();

    // ---- Counters / Uptime ----
    unsigned long getRestartCount();  // crash/unexpected restarts only
    unsigned long getBootCount();     // every boot, including clean ones
    String  getUptimeStr();

    bool isHealthy(); // false if any metric is currently WARNING/CRITICAL

    // JSON object for GET /api/health and for CloudRelay/dashboard use
    String getJson();

private:
    unsigned long lastCheck = 0;

    // Rolling CPU load estimate (simple exponential moving average so
    // one busy loop iteration — e.g. a sensor read — doesn't make the
    // gauge look falsely spiky).
    float cpuLoadEma = 0.0f;
    bool  haveLoopSample = false;

    // Edge-detect flags so threshold-crossing alerts fire once, same
    // convention as main.cpp's checkSensorAlerts().
    bool heapWasWarning = false;
    bool heapWasCritical = false;
    bool fsWasWarning = false;
    bool fsWasCritical = false;

    void checkThresholds();
};

extern HealthMonitor healthMonitor;

#endif
