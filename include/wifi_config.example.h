#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ==========================================
// LumiOS X - WiFi fallback list (EXAMPLE)
//
// 1. Copy this file to "wifi_config.h" in the
//    same folder.
// 2. Fill in your real SSID/password below.
// 3. wifi_config.h is git-ignored, so your real
//    credentials never get committed.
//
// This fallback list is only used if there is
// no saved WiFi in LittleFS yet (first boot, or
// after a failed connection to the saved one).
// Once the device connects successfully, that
// network is saved and used first on future boots.
// ==========================================

struct WiFiCredential
{
    const char *ssid;
    const char *password;
};

const WiFiCredential wifiList[] =
{
    { "YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD" },
};

const int WIFI_COUNT = sizeof(wifiList) / sizeof(wifiList[0]);

#endif
