#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>

// ==========================================
// LumiOS X - LED Manager
// ==========================================

// LED Automation Engine (Phase 2) — what each LED represents
// when the engine is driving it automatically. Matches the
// dashboard's LED Control Matrix labels (Power/WiFi/Sensor/System/Alert).
enum class LedRole
{
    NONE,
    HEARTBEAT,      // LED1 "Power"  — steady pulse while firmware is alive
    WIFI_STATUS,    // LED2 "WiFi"   — solid when connected, blinking while not
    SENSOR_STATE,   // LED3 "Sensor" — off/blink/solid for normal/warning/danger
    NOTIFICATION,   // LED4 "System" — brief pulse when a new notification fires
    MASTER_ALERT    // LED5 "Alert"  — solid when anything needs attention
};

class LEDManager
{
public:
    void begin();

    void on(uint8_t led);
    void off(uint8_t led);
    
    void toggle(uint8_t led);

    bool state(uint8_t led);
    
    void allOn();
    void allOff();

    void bootAnimation();
    void knightRider();
    void breathing();

    // ================================
    // LED Automation Engine (Phase 2)
    // Automatic -> Manual Override -> Automatic Resume
    // ================================

    // Enable/disable the whole automation engine. When disabled the
    // LEDs behave exactly like before this feature existed (pure manual).
    void setAutoMode(bool enabled);
    bool isAutoMode();
    String getAutoModeText(); // "Auto" / "Manual", for dashboard/OLED

    // Call every loop() when isAutoMode() is true. Non-blocking —
    // uses millis() timers only, never delay().
    void updateAuto();

    // Which role a given LED plays when auto mode is driving it.
    LedRole getRole(uint8_t led);
    static String roleText(LedRole role);

    // True while a given LED is under temporary manual override
    // (i.e. the user toggled it directly while auto mode was on).
    bool isOverridden(uint8_t led);

private:
    uint8_t getPin(uint8_t led);
    bool ledState[5] = {false, false, false, false, false};

    // Writes the pin + state without touching override bookkeeping.
    // Used internally by the automation engine.
    void rawWrite(uint8_t led, bool isOn);

    bool autoModeEnabled = true;

    // Per-LED manual override tracking. When the user manually flips an
    // LED while auto mode is on, that single LED "escapes" automation
    // for AUTO_RESUME_MS, then automatically rejoins the automation engine.
    bool manualOverride[5] = {false, false, false, false, false};
    unsigned long overrideStart[5] = {0, 0, 0, 0, 0};
    static const unsigned long AUTO_RESUME_MS = 30000; // 30s

    // Non-blocking blink timers, one per automated behaviour
    unsigned long lastHeartbeat = 0;
    bool heartbeatState = false;

    unsigned long lastWifiBlink = 0;
    bool wifiBlinkState = false;

    unsigned long lastSensorBlink = 0;
    bool sensorBlinkState = false;

    unsigned long notifyBlinkUntil = 0;
    unsigned long lastNotifyBlink = 0;
    bool notifyBlinkState = false;
    bool lastNotificationFlag = false;
};

// Global Object
extern LEDManager leds;

#endif