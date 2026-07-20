#include "health_monitor.h"
#include "config.h"
#include <LittleFS.h>

#include "sensor_manager.h"
#include "sensor_registry.h"
#include "wifi_manager.h"
#include "cloud_relay.h"
#include "storage_manager.h"
#include "notification_manager.h"
#include "system_manager.h"

HealthMonitor healthMonitor;

void HealthMonitor::begin()
{
    lastCheck = 0;
    cpuLoadEma = 0.0f;
}

// ==========================================
// Loop timing sample -> rolling CPU load estimate
//
// HONEST LIMITATION: the ESP32 Arduino core doesn't expose real
// per-task CPU utilization to sketch code without pulling in FreeRTOS
// runtime-stats instrumentation (which costs extra RAM/flash and a
// build-time config flag most LumiOS X users won't have enabled).
// This estimate instead measures how much of each loop() iteration
// was spent doing actual work vs. the trailing delay(200) — a
// reasonable proxy for "how busy is the main loop", not a true
// scheduler-level CPU percentage. Documented in ARCHITECTURE.md.
// ==========================================
void HealthMonitor::recordLoopTiming(unsigned long activeMs, unsigned long totalMs)
{
    if (totalMs == 0)
        return;

    float sample = (float)activeMs / (float)totalMs * 100.0f;
    if (sample > 100.0f) sample = 100.0f;

    if (!haveLoopSample)
    {
        cpuLoadEma = sample;
        haveLoopSample = true;
    }
    else
    {
        // EMA with alpha=0.2 — smooths spikes from an occasional slow
        // sensor read without lagging behind a genuine sustained rise.
        cpuLoadEma = (0.2f * sample) + (0.8f * cpuLoadEma);
    }
}

uint32_t HealthMonitor::getFreeHeap()
{
    return ESP.getFreeHeap();
}

uint8_t HealthMonitor::getHeapUsedPercent()
{
    uint32_t total = ESP.getHeapSize();
    if (total == 0) return 0;
    uint32_t used = total - ESP.getFreeHeap();
    return (uint8_t)((used * 100UL) / total);
}

uint8_t HealthMonitor::getFlashUsedPercent()
{
    uint32_t total = ESP.getFreeSketchSpace() + ESP.getSketchSize();
    if (total == 0) return 0;
    return (uint8_t)(((uint32_t)ESP.getSketchSize() * 100UL) / total);
}

uint8_t HealthMonitor::getFsUsedPercent()
{
    size_t total = LittleFS.totalBytes();
    if (total == 0) return 0;
    return (uint8_t)(((uint32_t)LittleFS.usedBytes() * 100UL) / total);
}

uint8_t HealthMonitor::getCpuLoadPercent()
{
    return (uint8_t)cpuLoadEma;
}

int HealthMonitor::getRSSI()
{
    return wifiManager.isConnected() ? wifiManager.getRSSI() : 0;
}

String HealthMonitor::getWiFiStatus()
{
    return wifiManager.getStateText();
}

String HealthMonitor::getMqttStatus()
{
    return cloudRelay.getStateText();
}

String HealthMonitor::getSensorStatus()
{
    bool coreOk = sensorManager.getStatus() == "Online";
    uint8_t pluginCount = sensorRegistry.count();

    if (!coreOk && pluginCount == 0)
        return "Offline";
    if (!coreOk)
        return "Partial (DHT offline)";
    if (sensorManager.hasActiveAlert())
        return "Alert Active";
    return "OK";
}

// Rough subsystem heartbeat summary — see HONEST LIMITATION note on
// recordLoopTiming(): the Arduino loop() model here is single-task,
// so "task status" is reported as each subsystem's own reachability
// rather than a real FreeRTOS task list.
String HealthMonitor::getTaskStatus()
{
    String s = "Loop:OK";
    s += " WiFi:" + String(wifiManager.isConnected() ? "OK" : "DOWN");
    s += " MQTT:" + String(cloudRelay.isConnected() ? "OK" : "DOWN");
    s += " Sensors:" + String(sensorManager.getStatus() == "Online" ? "OK" : "DOWN");
    return s;
}

unsigned long HealthMonitor::getRestartCount()
{
    return storageManager.getCrashCount();
}

unsigned long HealthMonitor::getBootCount()
{
    return storageManager.getBootCount();
}

String HealthMonitor::getUptimeStr()
{
    return systemManager.getUptime();
}

bool HealthMonitor::isHealthy()
{
    return !heapWasCritical && !fsWasCritical;
}

void HealthMonitor::checkThresholds()
{
    uint32_t freeHeap = getFreeHeap();
    bool heapCrit = freeHeap < HEALTH_HEAP_CRIT_BYTES;
    bool heapWarn = !heapCrit && freeHeap < HEALTH_HEAP_WARN_BYTES;

    if (heapCrit && !heapWasCritical)
        notificationManager.setNotification("Low memory: " + String(freeHeap / 1024) + " KB free", NotifPriority::CRITICAL);
    else if (heapWarn && !heapWasWarning && !heapCrit)
        notificationManager.setNotification("Memory getting low: " + String(freeHeap / 1024) + " KB free", NotifPriority::WARNING);

    heapWasCritical = heapCrit;
    heapWasWarning = heapWarn;

    uint8_t fsPct = getFsUsedPercent();
    bool fsCrit = fsPct >= HEALTH_FS_CRIT_PERCENT;
    bool fsWarn = !fsCrit && fsPct >= HEALTH_FS_WARN_PERCENT;

    if (fsCrit && !fsWasCritical)
        notificationManager.setNotification("Storage almost full: " + String(fsPct) + "% used", NotifPriority::CRITICAL);
    else if (fsWarn && !fsWasWarning && !fsCrit)
        notificationManager.setNotification("Storage usage high: " + String(fsPct) + "% used", NotifPriority::WARNING);

    fsWasCritical = fsCrit;
    fsWasWarning = fsWarn;
}

void HealthMonitor::update()
{
    unsigned long now = millis();
    if (now - lastCheck < HEALTH_CHECK_INTERVAL_MS)
        return;
    lastCheck = now;

    checkThresholds();
}

String HealthMonitor::getJson()
{
    String json = "{";
    json += "\"freeHeap\":" + String(getFreeHeap()) + ",";
    json += "\"heapUsedPercent\":" + String(getHeapUsedPercent()) + ",";
    json += "\"flashUsedPercent\":" + String(getFlashUsedPercent()) + ",";
    json += "\"fsUsedPercent\":" + String(getFsUsedPercent()) + ",";
    json += "\"cpuLoadPercent\":" + String(getCpuLoadPercent()) + ",";
    json += "\"rssi\":" + String(getRSSI()) + ",";
    json += "\"wifiStatus\":\"" + getWiFiStatus() + "\",";
    json += "\"mqttStatus\":\"" + getMqttStatus() + "\",";
    json += "\"sensorStatus\":\"" + getSensorStatus() + "\",";
    json += "\"taskStatus\":\"" + getTaskStatus() + "\",";
    json += "\"restartCount\":" + String(getRestartCount()) + ",";
    json += "\"bootCount\":" + String(getBootCount()) + ",";
    json += "\"uptime\":\"" + getUptimeStr() + "\",";
    json += "\"healthy\":" + String(isHealthy() ? "true" : "false");
    json += "}";
    return json;
}
