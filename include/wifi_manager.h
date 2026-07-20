#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// ==========================================
// LumiOS X - WiFi Manager (non-blocking)
//
// Connects using a small state machine driven
// from update() every loop() cycle. Never calls
// delay() or blocks — WiFi.begin() itself is
// async, so we just poll WiFi.status() over time.
// ==========================================

enum class WiFiConnState
{
    IDLE,               // about to start a fresh attempt cycle
    TRYING_SAVED,        // attempting stored LittleFS credentials
    TRYING_LIST,          // attempting wifiList[] fallback entries
    CONNECTED,
    COOLDOWN              // all attempts failed, waiting before retrying
};

class WiFiManagerX
{
public:
    void begin();      // kicks off the first connection attempt (non-blocking)
    void update();      // call every loop(); advances the state machine

    bool reconnect();   // force an immediate new attempt cycle
    bool isConnected();

    String getIP();
    String getSSID();
    int getRSSI();

    String getStateText(); // human-readable state, for dashboard/OLED

    // v1.0 — WiFi Recovery: exponential backoff status, for the
    // dashboard/OLED to show "retrying in Ns" instead of a flat state.
    unsigned int getReconnectAttempts();
    unsigned long getNextRetryMs(); // ms remaining until the next cooldown ends (0 if not cooling down)

private:
    WiFiConnState state = WiFiConnState::IDLE;

    String savedSSID;
    String savedPassword;
    bool hasSaved = false;

    int listIndex = -1;

    unsigned long attemptStart = 0;
    unsigned long cooldownStart = 0;
    unsigned long cooldownDurationMs = 0; // v1.0 — grows exponentially per consecutive failed cycle
    unsigned int  failedCycles = 0;       // v1.0 — reset to 0 on a successful connect

    bool everConnected = false;
    bool wasConnected = false;

    static const unsigned long CONNECT_TIMEOUT = 8000;  // ms per attempt

    void beginSavedAttempt();
    void beginListAttempt(int index);
    void advanceAfterTimeout();
};

extern WiFiManagerX wifiManager;

#endif
