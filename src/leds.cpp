#include "leds.h"
#include "config.h"

// Automation engine reads live state from these managers. No circular
// dependency — none of these headers include leds.h.
#include "wifi_manager.h"
#include "sensor_manager.h"
#include "notification_manager.h"

LEDManager leds;

// ================================
// Private
// ================================
uint8_t LEDManager::getPin(uint8_t led)
{
    switch (led)
    {
    case 1: return LED1_PIN;
    case 2: return LED2_PIN;
    case 3: return LED3_PIN;
    case 4: return LED4_PIN;
    case 5: return LED5_PIN;
    default: return LED1_PIN;
    }
}

// ================================
// Initialization
// ================================
void LEDManager::begin()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED4_PIN, OUTPUT);
    pinMode(LED5_PIN, OUTPUT);

    allOff();
}

// ================================
// Internal raw pin write (bypasses override bookkeeping —
// used by the automation engine itself)
// ================================
void LEDManager::rawWrite(uint8_t led, bool isOn)
{
    digitalWrite(getPin(led), isOn ? HIGH : LOW);

    if (led >= 1 && led <= 5)
        ledState[led - 1] = isOn;
}

// ================================
// Single LED (manual entry points)
// ================================
void LEDManager::on(uint8_t led)
{
    rawWrite(led, true);

    // A manual call while auto mode is running takes that one LED
    // out of automation for AUTO_RESUME_MS, then it rejoins on its own.
    if (autoModeEnabled && led >= 1 && led <= 5)
    {
        manualOverride[led - 1] = true;
        overrideStart[led - 1] = millis();
    }
}

void LEDManager::off(uint8_t led)
{
    rawWrite(led, false);

    if (autoModeEnabled && led >= 1 && led <= 5)
    {
        manualOverride[led - 1] = true;
        overrideStart[led - 1] = millis();
    }
}
// ================================
// All LEDs
// ================================
void LEDManager::allOn()
{
    for (int i = 1; i <= 5; i++)
    {
        on(i);
    }
}

void LEDManager::allOff()
{
    for (int i = 1; i <= 5; i++)
    {
        off(i);
    }
}

// ================================
// Boot Animation
// ================================
void LEDManager::bootAnimation()
{
    allOff();

    for (int i = 1; i <= 5; i++)
    {
        on(i);
        delay(180);
    }

    delay(300);

    allOff();
}

// ================================
// Knight Rider
// ================================
void LEDManager::knightRider()
{
    for (int i = 1; i <= 5; i++)
    {
        allOff();
        on(i);
        delay(80);
    }

    for (int i = 4; i >= 2; i--)
    {
        allOff();
        on(i);
        delay(80);
    }
}

// ================================
// Breathing
// ================================
void LEDManager::breathing()
{
    allOn();
    delay(250);

    allOff();
    delay(250);
}


// ================================

// ================================
void LEDManager::toggle(uint8_t led)
{
    if (state(led))
        off(led);
    else
        on(led);
}

bool LEDManager::state(uint8_t led)
{
    if (led >= 1 && led <= 5)
        return ledState[led - 1];

    return false;
}

// ================================================================
// LED Automation Engine (Phase 2)
//
// LED1 "Power"  -> HEARTBEAT     steady 1s pulse whenever firmware alive
// LED2 "WiFi"   -> WIFI_STATUS   solid = connected, blink = not connected
// LED3 "Sensor" -> SENSOR_STATE  off / slow blink (warning) / solid (danger)
// LED4 "System" -> NOTIFICATION  2s pulse whenever a new notification fires
// LED5 "Alert"  -> MASTER_ALERT  solid whenever anything needs attention
//                  (sensor warning/danger OR a warning/critical notification)
//
// Each LED can still be flipped manually at any time — doing so simply
// hands that one LED back to the user for AUTO_RESUME_MS, after which
// it quietly rejoins the automation engine.
// ================================================================

void LEDManager::setAutoMode(bool enabled)
{
    autoModeEnabled = enabled;
}

bool LEDManager::isAutoMode()
{
    return autoModeEnabled;
}

String LEDManager::getAutoModeText()
{
    return autoModeEnabled ? "Auto" : "Manual";
}

bool LEDManager::isOverridden(uint8_t led)
{
    if (led >= 1 && led <= 5)
        return manualOverride[led - 1];
    return false;
}

LedRole LEDManager::getRole(uint8_t led)
{
    switch (led)
    {
        case 1: return LedRole::HEARTBEAT;
        case 2: return LedRole::WIFI_STATUS;
        case 3: return LedRole::SENSOR_STATE;
        case 4: return LedRole::NOTIFICATION;
        case 5: return LedRole::MASTER_ALERT;
        default: return LedRole::NONE;
    }
}

String LEDManager::roleText(LedRole role)
{
    switch (role)
    {
        case LedRole::HEARTBEAT:    return "Heartbeat";
        case LedRole::WIFI_STATUS:  return "WiFi Status";
        case LedRole::SENSOR_STATE: return "Sensor State";
        case LedRole::NOTIFICATION: return "Notification";
        case LedRole::MASTER_ALERT: return "Master Alert";
        default: return "None";
    }
}

void LEDManager::updateAuto()
{
    if (!autoModeEnabled)
        return;

    unsigned long now = millis();

    // Let any LED whose manual-override window has expired rejoin automation.
    for (uint8_t i = 0; i < 5; i++)
    {
        if (manualOverride[i] && (now - overrideStart[i] >= AUTO_RESUME_MS))
            manualOverride[i] = false;
    }

    // ---- LED1: Heartbeat (Power) ----
    if (!manualOverride[0])
    {
        if (now - lastHeartbeat >= 1000)
        {
            lastHeartbeat = now;
            heartbeatState = !heartbeatState;
            rawWrite(1, heartbeatState);
        }
    }

    // ---- LED2: WiFi status ----
    if (!manualOverride[1])
    {
        if (wifiManager.isConnected())
        {
            rawWrite(2, true);
        }
        else if (now - lastWifiBlink >= 400)
        {
            lastWifiBlink = now;
            wifiBlinkState = !wifiBlinkState;
            rawWrite(2, wifiBlinkState);
        }
    }

    // ---- LED3: Sensor state ----
    bool sensorDanger = sensorManager.isSmokeDanger();
    bool sensorWarning = sensorManager.isSmokeWarning() || sensorManager.isDistanceWarning();

    if (!manualOverride[2])
    {
        if (sensorDanger)
        {
            rawWrite(3, true);
        }
        else if (sensorWarning)
        {
            if (now - lastSensorBlink >= 250)
            {
                lastSensorBlink = now;
                sensorBlinkState = !sensorBlinkState;
                rawWrite(3, sensorBlinkState);
            }
        }
        else
        {
            rawWrite(3, false);
        }
    }

    // ---- LED4: Notification pulse ----
    if (!manualOverride[3])
    {
        bool hasNotif = notificationManager.hasNotification();

        if (hasNotif && !lastNotificationFlag)
            notifyBlinkUntil = now + 2000; // pulse for 2s on a fresh notification

        lastNotificationFlag = hasNotif;

        if (now < notifyBlinkUntil)
        {
            if (now - lastNotifyBlink >= 150)
            {
                lastNotifyBlink = now;
                notifyBlinkState = !notifyBlinkState;
                rawWrite(4, notifyBlinkState);
            }
        }
        else
        {
            rawWrite(4, false);
        }
    }

    // ---- LED5: Master alert ----
    // Solid whenever a sensor is in warning/danger, or the current
    // notification was raised as WARNING/CRITICAL priority.
    if (!manualOverride[4])
    {
        NotifPriority p = notificationManager.getCurrentPriority();
        bool priorityAlert = notificationManager.hasNotification() &&
                              (p == NotifPriority::WARNING || p == NotifPriority::CRITICAL);

        rawWrite(5, sensorDanger || sensorWarning || priorityAlert);
    }
}