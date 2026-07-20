#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

class SettingsManager
{
public:
    void begin();

    // ==========================================
    // Settings Values
    // ==========================================

    bool developerMode = true;
    bool autoPage = true;
    bool animation = true;
    bool otaEnabled = true;
    bool resetPending = false;

    uint8_t brightness = 255;

    String theme = "Cyber";
    String language = "English";

    // ==========================================
    // Authentication (Stage H)
    // Off by default so the dashboard keeps working
    // out of the box. When enabled, sensitive
    // control routes (LED/restart/wifi/OTA) require
    // HTTP Basic Auth with these credentials.
    // ==========================================
    // NOTE: these are only the FACTORY-DEFAULT values used on first boot
    // (no /settings.txt in LittleFS yet). Once auth is toggled on/off or
    // credentials are changed via the dashboard, the real values are
    // persisted to flash by save() and reloaded by begin() on every
    // subsequent boot — see storage_manager.cpp saveSettings/loadSettings.
    bool authEnabled = false;
    String authUser = "admin";
    String authPass = "lumios123";

    void toggleAuth();
    String getAuth();
    bool setCredentials(String user, String pass);

    // Persist current settings to LittleFS. Called automatically by every
    // toggle/set method; exposed publicly too since webserver.cpp sets
    // settings.theme directly rather than through a setter.
    void save();

    // ==========================================
    // Toggle Functions
    // ==========================================

    void toggleDeveloperMode();
    void toggleAutoPage();
    void toggleAnimation();
    void toggleOTA();

    // ==========================================
    // Reset Control
    // ==========================================

    void requestReset();
    void reset();

    // ==========================================
    // Getters
    // ==========================================

    String getDeveloperMode();
    String getAutoPage();
    String getAnimation();
    String getOTA();

    String getBrightness();
    String getTheme();
    String getLanguage();
    String getReset();
};

extern SettingsManager settings;

#endif