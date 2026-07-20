#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// ==========================================
// LumiOS X - Display Manager
// ==========================================

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C display;

// Available Pages
enum DisplayPage
{
    PAGE_HOME,
    PAGE_WIFI,
    PAGE_SYSTEM,
    PAGE_WEATHER,
    PAGE_DEVELOPER,
    PAGE_NOTIFICATIONS,
    PAGE_SETTINGS,
    PAGE_PLUGINS, // Phase 6 — auto-populated from SensorRegistry, appended so
                  // no existing page's numeric value changes
    PAGE_HEALTH   // v1.0 — Health Monitor summary, appended for the same reason
};

// Initialization
void initDisplay();

// Boot
void showBootScreen();

// Display Manager
void setPage(DisplayPage page);
void nextPage();
void previousPage();
void updateDisplay();


// Individual Pages
void drawHomePage();
void drawWiFiPage();
void drawSystemPage();
void drawWeatherPage();
void drawNotificationsPage();
void drawAlertOverlay(); // short full-screen alert shown immediately when a
                          // new notification fires, independent of page rotation
void drawSettingsPage();
void drawPluginsPage();
void drawDeveloperPage();
void drawHealthPage(); // v1.0
String formatUptime();

// v1.0 — OTA progress screen. Called directly (like showBootScreen())
// from updateDisplay() while OTAManager reports an upload in
// progress; takes over the OLED for the duration so a firmware flash
// is never silently invisible to someone standing at the device.
void showOTAScreen();



String getCurrentPageName();

#endif