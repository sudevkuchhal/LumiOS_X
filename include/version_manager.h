#ifndef VERSION_MANAGER_H
#define VERSION_MANAGER_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — Version Manager (v1.0)
//
// Single source of truth for "what exactly is running right now" —
// firmware/API/dashboard/OLED versions, build date/time (compiler
// __DATE__/__TIME__, baked in at compile time), and ESP32 chip/flash
// facts. Read-only reporting module; the actual version numbers live
// in config.h (FIRMWARE_VERSION / API_VERSION / DASHBOARD_VERSION /
// OLED_VERSION) so bumping one is a one-line change there.
// ==========================================================

class VersionManager
{
public:
    void begin();

    String getFirmwareVersion();
    String getBuildDate();   // e.g. "Jul 20 2026"
    String getBuildTime();   // e.g. "14:32:01"
    String getChipModel();
    String getFlashSizeStr();  // total flash, e.g. "4 MB"
    String getFreeFlashStr();  // free (unused by app partition) sketch space
    String getApiVersion();
    String getDashboardVersion();
    String getOledVersion();

    // Full JSON object (no outer array) for GET /api/version
    String getJson();

private:
    String buildDate;
    String buildTime;
};

extern VersionManager versionManager;

#endif
