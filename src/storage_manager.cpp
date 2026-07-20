#include "storage_manager.h"
#include "config.h"
#include <LittleFS.h>

StorageManager storageManager;

#define WIFI_FILE "/wifi.txt"
#define SETTINGS_FILE "/settings.txt"

bool StorageManager::begin()
{
    return LittleFS.begin(true);
}

bool StorageManager::saveWiFi(String ssid, String password)
{
    File file = LittleFS.open(WIFI_FILE, "w");

    if (!file)
    {
        return false;
    }

    file.println(ssid);
    file.println(password);

    file.close();

    return true;
}

bool StorageManager::loadWiFi(String &ssid, String &password)
{
    if (!LittleFS.exists(WIFI_FILE))
    {
        return false;
    }

    File file = LittleFS.open(WIFI_FILE, "r");

    if (!file)
    {
        return false;
    }

    ssid = file.readStringUntil('\n');
    password = file.readStringUntil('\n');

    ssid.trim();
    password.trim();

    file.close();

    return true;
}

// ==========================================
// Boot Counter
// ==========================================

unsigned long StorageManager::incrementBootCount()
{
    unsigned long count = 0;

    if (LittleFS.exists(BOOTCOUNT_FILE))
    {
        File file = LittleFS.open(BOOTCOUNT_FILE, "r");
        if (file)
        {
            count = file.parseInt();
            file.close();
        }
    }

    count++;
    bootCount = count;

    File out = LittleFS.open(BOOTCOUNT_FILE, "w");
    if (out)
    {
        out.print(count);
        out.close();
    }

    return count;
}

unsigned long StorageManager::getBootCount()
{
    return bootCount;
}

// ==========================================
// Crash Counter (v1.0 Health Monitor)
// Same on-disk pattern as the boot counter above.
// ==========================================

unsigned long StorageManager::incrementCrashCount()
{
    unsigned long count = 0;

    if (LittleFS.exists(CRASHCOUNT_FILE))
    {
        File file = LittleFS.open(CRASHCOUNT_FILE, "r");
        if (file)
        {
            count = file.parseInt();
            file.close();
        }
    }

    count++;
    crashCount = count;

    File out = LittleFS.open(CRASHCOUNT_FILE, "w");
    if (out)
    {
        out.print(count);
        out.close();
    }

    return count;
}

unsigned long StorageManager::getCrashCount()
{
    if (crashCount == 0 && LittleFS.exists(CRASHCOUNT_FILE))
    {
        File file = LittleFS.open(CRASHCOUNT_FILE, "r");
        if (file)
        {
            crashCount = file.parseInt();
            file.close();
        }
    }
    return crashCount;
}

// ==========================================
// Settings Persistence (Stage H security fix)
//
// Previously SettingsManager::begin() did nothing, so authEnabled /
// authUser / authPass always reset to the compiled-in defaults on every
// reboot — meaning enabling auth and setting a custom password didn't
// survive a restart (including the auto-restart after an OTA update).
// This gives every setting a home in flash, key=value style so it's
// easy to read/extend without a JSON dependency.
// ==========================================

bool StorageManager::saveSettings(bool authEnabled, const String &authUser, const String &authPass,
                                   bool developerMode, bool autoPage, bool animation, bool otaEnabled,
                                   uint8_t brightness, const String &theme, const String &language)
{
    File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file)
        return false;

    file.printf("authEnabled=%d\n", authEnabled ? 1 : 0);
    file.printf("authUser=%s\n", authUser.c_str());
    file.printf("authPass=%s\n", authPass.c_str());
    file.printf("developerMode=%d\n", developerMode ? 1 : 0);
    file.printf("autoPage=%d\n", autoPage ? 1 : 0);
    file.printf("animation=%d\n", animation ? 1 : 0);
    file.printf("otaEnabled=%d\n", otaEnabled ? 1 : 0);
    file.printf("brightness=%d\n", brightness);
    file.printf("theme=%s\n", theme.c_str());
    file.printf("language=%s\n", language.c_str());

    file.close();
    return true;
}

bool StorageManager::loadSettings(bool &authEnabled, String &authUser, String &authPass,
                                   bool &developerMode, bool &autoPage, bool &animation, bool &otaEnabled,
                                   uint8_t &brightness, String &theme, String &language)
{
    if (!LittleFS.exists(SETTINGS_FILE))
        return false; // first boot — caller keeps its defaults

    File file = LittleFS.open(SETTINGS_FILE, "r");
    if (!file)
        return false;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        int eq = line.indexOf('=');
        if (eq <= 0)
            continue;

        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);

        if (key == "authEnabled") authEnabled = (value == "1");
        else if (key == "authUser") authUser = value;
        else if (key == "authPass") authPass = value;
        else if (key == "developerMode") developerMode = (value == "1");
        else if (key == "autoPage") autoPage = (value == "1");
        else if (key == "animation") animation = (value == "1");
        else if (key == "otaEnabled") otaEnabled = (value == "1");
        else if (key == "brightness") brightness = (uint8_t)value.toInt();
        else if (key == "theme") theme = value;
        else if (key == "language") language = value;
    }

    file.close();
    return true;
}