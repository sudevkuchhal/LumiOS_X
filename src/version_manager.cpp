#include "version_manager.h"
#include "config.h"
#include <Arduino.h>

VersionManager versionManager;

void VersionManager::begin()
{
    buildDate = String(__DATE__);
    buildTime = String(__TIME__);
}

String VersionManager::getFirmwareVersion() { return FIRMWARE_VERSION; }
String VersionManager::getBuildDate()       { return buildDate; }
String VersionManager::getBuildTime()       { return buildTime; }
String VersionManager::getApiVersion()      { return API_VERSION; }
String VersionManager::getDashboardVersion(){ return DASHBOARD_VERSION; }
String VersionManager::getOledVersion()     { return OLED_VERSION; }

String VersionManager::getChipModel()
{
    return String(ESP.getChipModel()) + " Rev " + String(ESP.getChipRevision());
}

String VersionManager::getFlashSizeStr()
{
    return String(ESP.getFlashChipSize() / 1024 / 1024) + " MB";
}

// "Free flash" here means unused space in the currently running app
// partition (how much bigger the sketch could grow) — the figure a
// developer actually cares about day to day. Total OTA headroom
// (whether a same-size second app partition exists) is a partition
// table fact, not a runtime one; see ARCHITECTURE.md for the
// partition layout this assumes.
String VersionManager::getFreeFlashStr()
{
    uint32_t free = ESP.getFreeSketchSpace();
    return String(free / 1024) + " KB";
}

String VersionManager::getJson()
{
    String json = "{";
    json += "\"firmware\":\"" + getFirmwareVersion() + "\",";
    json += "\"buildDate\":\"" + getBuildDate() + "\",";
    json += "\"buildTime\":\"" + getBuildTime() + "\",";
    json += "\"chip\":\"" + getChipModel() + "\",";
    json += "\"flashSize\":\"" + getFlashSizeStr() + "\",";
    json += "\"freeFlash\":\"" + getFreeFlashStr() + "\",";
    json += "\"apiVersion\":\"" + getApiVersion() + "\",";
    json += "\"dashboardVersion\":\"" + getDashboardVersion() + "\",";
    json += "\"oledVersion\":\"" + getOledVersion() + "\"";
    json += "}";
    return json;
}
