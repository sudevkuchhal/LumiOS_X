#include "settings.h"
#include "storage_manager.h"
#include "event_logger.h"

SettingsManager settings;

// ==========================================
// Initialization
// ==========================================

void SettingsManager::begin()
{
    // Load persisted settings if they exist; otherwise the compiled-in
    // defaults above (auth OFF, admin/lumios123, theme "Cyber", ...)
    // stay in effect for this boot AND get written out below so the
    // very next reboot has a real settings file to read.
    bool loaded = storageManager.loadSettings(
        authEnabled, authUser, authPass,
        developerMode, autoPage, animation, otaEnabled,
        brightness, theme, language);

    if (!loaded)
        save();
}

void SettingsManager::save()
{
    storageManager.saveSettings(
        authEnabled, authUser, authPass,
        developerMode, autoPage, animation, otaEnabled,
        brightness, theme, language);

    eventLogger.log(EventType::SETTINGS_CHANGED,
        "theme=" + theme + " dev=" + String(developerMode) + " ota=" + String(otaEnabled) + " auth=" + String(authEnabled));
}

// ==========================================
// Toggle Functions
// ==========================================

void SettingsManager::toggleDeveloperMode()
{
    developerMode = !developerMode;
    save();
}

void SettingsManager::toggleAutoPage()
{
    autoPage = !autoPage;
    save();
}

void SettingsManager::toggleAnimation()
{
    animation = !animation;
    save();
}

void SettingsManager::toggleOTA()
{
    otaEnabled = !otaEnabled;
    save();
}

void SettingsManager::toggleAuth()
{
    authEnabled = !authEnabled;
    save();
}

String SettingsManager::getAuth()
{
    return authEnabled ? "ON" : "OFF";
}

bool SettingsManager::setCredentials(String user, String pass)
{
    // Raised from 4 -> 8 chars minimum (Stage H hardening). Still not a
    // strength check (no complexity rules), just a floor against trivial
    // passwords — this is HTTP Basic Auth over plain HTTP either way, so
    // treat it as a deterrent, not real protection, until TLS is added.
    if (user.length() == 0 || pass.length() < 8)
        return false;

    authUser = user;
    authPass = pass;
    save();
    return true;
}

// ==========================================
// Reset Control
// ==========================================

void SettingsManager::requestReset()
{
    resetPending = true;
}

// ==========================================
// Getters
// ==========================================

String SettingsManager::getDeveloperMode()
{
    return developerMode ? "ON" : "OFF";
}

String SettingsManager::getAutoPage()
{
    return autoPage ? "ON" : "OFF";
}

String SettingsManager::getAnimation()
{
    return animation ? "ON" : "OFF";
}

String SettingsManager::getOTA()
{
    return otaEnabled ? "ON" : "OFF";
}

String SettingsManager::getBrightness()
{
    return String(brightness);
}

String SettingsManager::getTheme()
{
    return theme;
}

String SettingsManager::getLanguage()
{
    return language;
}

String SettingsManager::getReset()
{
    return resetPending ? "Pending" : "Idle";
}

// ==========================================
// Reset
// ==========================================

void SettingsManager::reset()
{
    developerMode = true;
    autoPage = true;
    animation = true;
    otaEnabled = true;

    brightness = 255;

    theme = "Cyber";
    language = "English";

    // Factory reset also clears auth back to the compiled-in default —
    // deliberate: if someone hits physical/dashboard reset, they should
    // not be locked out by a forgotten custom password.
    authEnabled = false;
    authUser = "admin";
    authPass = "lumios123";

    resetPending = false;
    save();
}