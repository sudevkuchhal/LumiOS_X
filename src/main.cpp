#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>

#include "config.h"
#include "developer_api.h"
#include "display.h"
#include "boot.h"
#include "leds.h"
#include "webserver.h"

#include "settings.h"
#include "sensor_manager.h"
#include "wifi_manager.h"
#include "system_manager.h"
#include "notification_manager.h"
#include "storage_manager.h"
#include "cloud_relay.h"
#include "sensor_registry.h"
#include "version_manager.h"
#include "health_monitor.h"
#include "error_logger.h"
#include "event_logger.h"
#include "ota_manager.h"

void setup()
{
    Serial.begin(SERIAL_BAUDRATE);
    delay(200);

    settings.begin();

    Serial.println();
    Serial.println("==============================");
    Serial.println(PROJECT_NAME);
    Serial.println(PROJECT_VERSION);
    Serial.println("==============================");

    // Initialize Storage Manager
    if (storageManager.begin())
    {
        Serial.println("Storage Manager Ready");

        // v1.0 — Error/Event Logger need LittleFS mounted, so they come
        // up right after StorageManager, before anything that might
        // fail and want to log it.
        errorLogger.begin();
        eventLogger.begin();

        unsigned long bootCount = storageManager.incrementBootCount();
        Serial.print("Boot Count: ");
        Serial.println(bootCount);

        // v1.0 Health Monitor — distinguish a crash/watchdog/brownout
        // restart from a normal power-on or software restart, so
        // "Restart Count" on the dashboard means something different
        // from the raw boot counter.
        String resetReason = systemManager.getResetReason();
        bool wasCrash = (resetReason == "Crash / Panic" ||
                          resetReason == "Interrupt Watchdog" ||
                          resetReason == "Task Watchdog" ||
                          resetReason == "Other Watchdog" ||
                          resetReason == "Brownout (Power Dip)");
        if (wasCrash)
        {
            unsigned long crashes = storageManager.incrementCrashCount();
            Serial.print("Unexpected Restart Count: ");
            Serial.println(crashes);
            errorLogger.log(ErrorCategory::RUNTIME, "Unexpected restart: " + resetReason);
        }

        eventLogger.log(EventType::BOOT, "reason=" + resetReason + " boot#" + String(bootCount));

        // Test Save WiFi
        
        // Test Load WiFi
        String ssid;
        String password;

        if (storageManager.loadWiFi(ssid, password))
        {
            Serial.println("========== Saved WiFi ==========");
            Serial.print("SSID : ");
            Serial.println(ssid);
            Serial.println("PASS : [hidden]");
            Serial.println("===============================");
        }
        else
        {
            Serial.println("No Saved WiFi");
        }
    }
    else
    {
        Serial.println("Storage Manager Failed");
    }

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    developerAPI.begin();

    leds.begin();

    initDisplay();

    boot.begin();

    wifiManager.begin();

    

    

    versionManager.begin();
    healthMonitor.begin();
    otaManager.begin();

    sensorManager.begin();

    // Plugin sensors (Phase 6) — zero registered today, but any future
    // SensorName.h/.cpp with REGISTER_SENSOR() at the bottom will show
    // up here automatically, no edit to this file required.
    sensorRegistry.beginAll();

    systemManager.begin();

    notificationManager.begin();
    
    webServer.begin();

    cloudRelay.begin();

    notificationManager.setNotification("Boot Complete - Connecting WiFi...");

    Serial.println("Boot Complete");
}

// Edge-detection flags so alert notifications fire once when a
// threshold is crossed, not every 200ms while it stays crossed.
static bool wasDistanceWarning = false;
static bool wasSmokeWarning = false;
static bool wasSmokeDanger = false;

void checkSensorAlerts()
{
    bool distWarn = sensorManager.isDistanceWarning();
    bool smokeWarn = sensorManager.isSmokeWarning();
    bool smokeDanger = sensorManager.isSmokeDanger();

    if (smokeDanger && !wasSmokeDanger)
    {
        notificationManager.setNotification("DANGER: High smoke/gas level detected!", NotifPriority::CRITICAL);
    }
    else if (smokeWarn && !wasSmokeWarning && !smokeDanger)
    {
        notificationManager.setNotification("Warning: Smoke/gas level rising", NotifPriority::WARNING);
    }

    if (distWarn && !wasDistanceWarning)
    {
        notificationManager.setNotification("Object detected close to sensor", NotifPriority::WARNING);
    }

    wasDistanceWarning = distWarn;
    wasSmokeWarning = smokeWarn;
    wasSmokeDanger = smokeDanger;
}

void loop()
{
    unsigned long loopStart = millis();

    developerAPI.update();

    updateDisplay();

    healthMonitor.update();

    sensorManager.update();

    sensorRegistry.updateAll(); // no-op today; handles future plugin sensors

    checkSensorAlerts();

    systemManager.update();

    notificationManager.update();

    wifiManager.update();

    cloudRelay.update();

    // LED Automation Engine (Phase 2) — non-blocking, reflects WiFi /
    // sensor / notification state on the physical LEDs unless a
    // specific LED is under temporary manual override.
    leds.updateAuto();

    // v1.0 Health Monitor — feed this iteration's active work time
    // (before the trailing delay below) into the rolling CPU load
    // estimate. See health_monitor.cpp for what this estimate can and
    // can't promise.
    unsigned long activeMs = millis() - loopStart;
    healthMonitor.recordLoopTiming(activeMs, activeMs + 200);

    delay(200);
}