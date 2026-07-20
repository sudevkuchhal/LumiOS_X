#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>

class StorageManager
{
public:
    bool begin();

    bool saveWiFi(String ssid, String password);

    bool loadWiFi(String &ssid, String &password);

    // Boot counter — persisted across reboots in LittleFS.
    // Call once during setup(); returns the new (incremented) count.
    unsigned long incrementBootCount();
    unsigned long getBootCount();

    // Crash/unexpected-restart counter (v1.0 Health Monitor).
    // Separate from getBootCount() (which counts EVERY boot, including
    // clean OTA/manual restarts) so the dashboard can distinguish
    // "restarted 40 times because I keep flashing new firmware" from
    // "restarted 40 times because it keeps crashing". Caller decides
    // what counts as a crash (see SystemManager::getResetReason()) and
    // only calls incrementCrashCount() for panic/brownout/watchdog
    // resets.
    unsigned long incrementCrashCount();
    unsigned long getCrashCount();

    // Settings persistence (Stage H security fix).
    // Simple key=value text file — mirrors the plain-text style already
    // used by saveWiFi/loadWiFi rather than pulling in a JSON library.
    // Called by SettingsManager itself; nothing else should touch this file.
    bool saveSettings(bool authEnabled, const String &authUser, const String &authPass,
                       bool developerMode, bool autoPage, bool animation, bool otaEnabled,
                       uint8_t brightness, const String &theme, const String &language);

    // Returns false if no settings file exists yet (first boot) — caller
    // should keep its compiled-in defaults in that case.
    bool loadSettings(bool &authEnabled, String &authUser, String &authPass,
                       bool &developerMode, bool &autoPage, bool &animation, bool &otaEnabled,
                       uint8_t &brightness, String &theme, String &language);

private:
    unsigned long bootCount = 0;
    unsigned long crashCount = 0;
};

extern StorageManager storageManager;

#endif
