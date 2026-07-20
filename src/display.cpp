#include <esp_system.h>
#include "display.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include "developer_api.h"
#include "settings.h"
#include "sensor_manager.h"
#include "system_manager.h"
#include "notification_manager.h"
#include "sensor_registry.h"
#include "health_monitor.h"
#include "ota_manager.h"






U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0);
// ==========================================
// Display Manager
// ==========================================

DisplayPage currentPage = PAGE_HOME;

unsigned long lastPageChange = 0;

const unsigned long PAGE_INTERVAL = 3000;




void initDisplay()
{
    // Fast-mode I2C (400kHz) instead of the 100kHz default — reduces
    // the time each sendBuffer() takes to push the full 128x64 frame,
    // which is what actually helps with sluggish/blurry-feeling refreshes
    // on SH1106 modules (default speed just moves data slower, it doesn't
    // change what gets drawn).
    Wire.begin();
    Wire.setClock(400000);

    display.begin();
    display.clearBuffer();
    display.sendBuffer();
}





void showBootScreen()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(30, 24, "LumiOS");

    display.setFont(u8g2_font_6x12_tr);
    display.drawStr(22, 45, "ESP32 Control Hub");
    display.drawStr(28, 60, "Booting...");

    display.sendBuffer();

    delay(2000);
}




// ==========================================
// Display Manager
// ==========================================



void setPage(DisplayPage page)
{
    currentPage = page;
}

void nextPage()
{
    currentPage = (DisplayPage)((currentPage + 1) % 9); // 9 pages now that PAGE_HEALTH exists
}

void previousPage()
{
    if (currentPage == PAGE_HOME)
        currentPage = PAGE_HEALTH; // wrap to the new last page
    else
        currentPage = (DisplayPage)(currentPage - 1);
}

// ==========================================
// Notification -> OLED sync (Phase 4)
//
// Previously the OLED only showed a notification if the auto-rotating
// page carousel happened to be sitting on PAGE_NOTIFICATIONS when it
// fired - a notification could appear and fully expire (5s) without
// ever being seen on the OLED, even though the dashboard had it
// instantly via /api/status. This overlay makes the OLED react the
// moment notificationManager.setNotification() is called, regardless
// of whatever page is currently showing, then hands control back to
// the normal page rotation exactly where it left off.
// ==========================================

static uint32_t lastShownNotifSeq = 0;
static bool alertOverlayActive = false;
static unsigned long alertOverlayStart = 0;
const unsigned long ALERT_OVERLAY_DURATION = 2500; // short alert, not a full page

void updateDisplay()
{
    // v1.0 — OTA priority lock: while a firmware upload is in
    // progress, the OLED shows nothing else. This mirrors the
    // existing alert-overlay pattern below but takes priority over it
    // (a stray sensor notification firing mid-flash shouldn't hide
    // upload progress). Reads OTAManager state only — never touches
    // Update.h/I2C from anywhere but this main-loop-driven function,
    // so there's no cross-task display contention with the async
    // upload handler in webserver.cpp.
    if (otaManager.isActive())
    {
        showOTAScreen();
        return;
    }

    // A new notification arrived since we last checked -> interrupt
    // whatever page is showing with a short alert overlay.
    uint32_t seq = notificationManager.getNotificationSeq();
    if (seq != lastShownNotifSeq)
    {
        lastShownNotifSeq = seq;
        alertOverlayActive = true;
        alertOverlayStart = millis();
    }

    if (alertOverlayActive)
    {
        if (millis() - alertOverlayStart < ALERT_OVERLAY_DURATION)
        {
            drawAlertOverlay();
            return; // page rotation clock is untouched/paused while the alert shows
        }

        // Overlay just finished — resume rotation with a fresh full
        // PAGE_INTERVAL for whichever page comes next, instead of
        // inheriting whatever was left on the pre-overlay clock.
        alertOverlayActive = false;
        lastPageChange = millis();
    }

    if (millis() - lastPageChange >= PAGE_INTERVAL)
    {
        lastPageChange = millis();
        nextPage();
    }

    switch (currentPage)
    {
        case PAGE_HOME:
            drawHomePage();
            break;

        case PAGE_WIFI:
            drawWiFiPage();
            break;

        case PAGE_SYSTEM:
            drawSystemPage();
            break;

        case PAGE_WEATHER:
            drawWeatherPage();
            break;

        case PAGE_DEVELOPER:
            drawDeveloperPage();
            break;

        case PAGE_NOTIFICATIONS:
            drawNotificationsPage();
            break;

        case PAGE_SETTINGS:
            drawSettingsPage();
            break;

        case PAGE_PLUGINS:
            drawPluginsPage();
            break;

        case PAGE_HEALTH:
            drawHealthPage();
            break;
    }
}
// ==========================================
// OLED Pages
// ==========================================

void drawHomePage()
{
    display.clearBuffer();

    // Title
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(20,12,"LumiOS X");

    display.drawLine(0,15,128,15);

    display.setFont(u8g2_font_6x12_tr);

    // System
    display.setCursor(0,30);
    display.print("SYS :  ONLINE");

    // WiFi
    display.setCursor(0,42);

    if (wifiManager.isConnected())
    {
        display.print("WiFi: OK");
    }
    else
    {
        display.print("WiFi: ---");
    }

    // Heap
    display.setCursor(0,54);
    display.print("Heap:");
    display.print(ESP.getFreeHeap()/1024);
    display.print("KB");

    // Uptime
    display.setCursor(0,62);
    display.print("Up:");
    display.print(millis()/1000);
    display.print("s");

    display.sendBuffer();
}

void drawWiFiPage()
{
    display.clearBuffer();

    // Title
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(35, 14, "WIFI");

    display.drawLine(0, 18, 128, 18);

    display.setFont(u8g2_font_6x12_tr);

    if (wifiManager.isConnected())
    {
        String ssid = wifiManager.getSSID();
        String ip   = wifiManager.getIP();

        char buffer[32];

        display.drawStr(0, 30, ("SSID : " + ssid).c_str());

        display.drawStr(0, 42, ("IP   : " + ip).c_str());

        sprintf(buffer, "RSSI : %d dBm", wifiManager.getRSSI());
        display.drawStr(0, 54, buffer);

        display.drawStr(0, 62, " ONLINE");
    }
    else
    {
        display.drawStr(0, 35, "WiFi Not Connected");
        display.drawStr(0, 50, "Waiting...");
    }

    display.sendBuffer();
}

void drawSystemPage()
{
    display.clearBuffer();

    // ==========================
    // Title
    // ==========================

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(20, 14, "SYSTEM");

    display.drawLine(0, 18, 128, 18);

    // ==========================
    // Live System Information
    // ==========================

    display.setFont(u8g2_font_6x12_tr);

    display.drawStr(0, 30,
        ("Heap : " + systemManager.getHeap()).c_str());

    display.drawStr(0, 40,
        ("Flash: " + systemManager.getFlash()).c_str());

    display.drawStr(0, 50,
        ("CPU  : " + systemManager.getCPU()).c_str());

    display.drawStr(0, 60,
        ("UP   : " + systemManager.getUptime()).c_str());

    display.sendBuffer();
}

// ==========================================
// Format Uptime
// ==========================================

String formatUptime()
{
    unsigned long seconds = millis() / 1000;

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    char timeString[16];

    sprintf(timeString, "%02d:%02d:%02d",
            hours,
            minutes,
            secs);

    return String(timeString);


    
}
String getCurrentPageName()
{
    switch(currentPage)
    {
        case PAGE_HOME:
            return "HOME";

        case PAGE_WIFI:
            return "WIFI";

        case PAGE_SYSTEM:
            return "SYSTEM";

        case PAGE_WEATHER:
            return "SENSORS";

        case PAGE_DEVELOPER:
            return "DEVELOPER";

        case PAGE_NOTIFICATIONS:
            return "NOTIFICATIONS";

        case PAGE_SETTINGS:
            return "SETTINGS";

        case PAGE_PLUGINS:
            return "PLUGINS";

        case PAGE_HEALTH:
            return "HEALTH";

        default:
            return "UNKNOWN";
    }
}




void drawWeatherPage()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(25,15,"Weather");

    display.drawLine(0,18,128,18);

    display.setFont(u8g2_font_6x12_tr);

char buffer[32];

sprintf(buffer, "Temp : %.1f C", sensorManager.getTemperature());
display.drawStr(0, 34, buffer);

sprintf(buffer, "Hum  : %.1f %%", sensorManager.getHumidity());
display.drawStr(0, 46, buffer);

display.drawStr(0, 58, ("Status : " + sensorManager.getStatus()).c_str());

    display.sendBuffer();
}

void drawDeveloperPage()
{
    static uint8_t page = 0;

    static unsigned long lastChange = 0;

    if (millis() - lastChange > 5000)
    {
        page++;
        if (page > 3)
            page = 0;

        lastChange = millis();
    }

    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);

    display.drawStr(8,15,"Developer");

    display.drawLine(0,18,128,18);

    display.setFont(u8g2_font_6x12_tr);

    switch(page)
    {
        case 0:

            display.drawStr(0,35,"Sudev Kuchhal");
            display.drawStr(0,50,"Embedded Systems");
            break;

        case 1:
{
    char buffer[32];

    display.drawStr(0,30,"GitHub");

    sprintf(buffer,"Repos : %d", developerAPI.getRepos());
    display.drawStr(0,45,buffer);

    sprintf(buffer,"Stars : %d", developerAPI.getStars());
    display.drawStr(0,60,buffer);

    break;
}

        case 2:

            display.drawStr(0,35,"LinkedIn");
            display.drawStr(0,50,"linkedin.com/in/");
            display.drawStr(0,62,"sudevkuchhal");
            break;

case 3:

    display.drawStr(0,30,"Status");
    display.drawStr(0,50,developerAPI.getStatus().c_str());

    break;
    }

    display.sendBuffer();
}

// ==========================================
// Alert Overlay — short, full-screen, priority-styled
// (matches the OLED examples in the spec: "WiFi Connected",
// "Temperature High", "Door Open", "Smoke Detected", "OTA Success")
// ==========================================

// Very small word-wrapper for the 6x12 font (~6px/char, 128px wide
// screen -> ~21 chars/line). Splits on the nearest preceding space so
// words aren't cut mid-word; falls back to a hard cut if there's no
// space to break on.
static void wrapTwoLines(const String &msg, String &line1, String &line2)
{
    const uint8_t maxChars = 21;

    if (msg.length() <= maxChars)
    {
        line1 = msg;
        line2 = "";
        return;
    }

    int breakAt = msg.lastIndexOf(' ', maxChars);
    if (breakAt <= 0) breakAt = maxChars;

    line1 = msg.substring(0, breakAt);
    line2 = msg.substring(breakAt + 1);

    if (line2.length() > maxChars)
        line2 = line2.substring(0, maxChars - 1) + "\x85"; // ellipsis-ish cut
}

void drawAlertOverlay()
{
    display.clearBuffer();

    NotifPriority priority = notificationManager.getCurrentPriority();
    String message = notificationManager.getCurrentNotification();

    // CRITICAL gets an inverted (filled) banner so it reads as urgent
    // at a glance even without color; WARNING gets a boxed outline;
    // INFO is plain text, consistent with the dashboard's severity styling.
    const char *label = "NOTICE";
    if (priority == NotifPriority::CRITICAL) label = "! CRITICAL !";
    else if (priority == NotifPriority::WARNING) label = "WARNING";

    if (priority == NotifPriority::CRITICAL)
    {
        display.drawBox(0, 0, 128, 16);
        display.setDrawColor(0); // black text on white banner
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(64 - (strlen(label) * 4), 12, label);
        display.setDrawColor(1);
    }
    else
    {
        if (priority == NotifPriority::WARNING)
            display.drawFrame(0, 0, 128, 16);

        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(64 - (strlen(label) * 3), 12, label);
    }

    display.drawLine(0, 20, 128, 20);

    String line1, line2;
    wrapTwoLines(message, line1, line2);

    display.setFont(u8g2_font_6x12_tr);
    display.drawStr(2, 36, line1.c_str());
    if (line2.length())
        display.drawStr(2, 50, line2.c_str());

    display.sendBuffer();
}

void drawNotificationsPage()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(8,14,"NOTIFICATIONS");

    display.drawLine(0,18,128,18);

    display.setFont(u8g2_font_6x12_tr);

    display.drawStr(
        0,
        40,
        notificationManager.getCurrentNotification().c_str());

    display.sendBuffer();
}



void drawSettingsPage()
{
    static uint8_t page = 0;
    static unsigned long lastChange = 0;

    if (millis() - lastChange > 5000)
    {
        page++;

        if(page > 1)
            page = 0;

        lastChange = millis();
    }

    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(25,14,"SETTINGS");
    display.drawLine(0,18,128,18);

    display.setFont(u8g2_font_6x12_tr);

    switch(page)
    {
        case 0:

            display.drawStr(0,30,("Developer : " + settings.getDeveloperMode()).c_str());

            display.drawStr(0,40,("Auto Page : " + settings.getAutoPage()).c_str());

            display.drawStr(0,50,("Animation : " + settings.getAnimation()).c_str());

            display.drawStr(0,60,("OTA : " + settings.getOTA()).c_str());

        break;

        case 1:

            display.drawStr(0,30,("Brightness : " + settings.getBrightness()).c_str());

            display.drawStr(0,40,("Theme : " + settings.getTheme()).c_str());

            display.drawStr(0,50,("Language : " + settings.getLanguage()).c_str());

            display.drawStr(0,60,("Reset : " + settings.getReset()).c_str());

        break;
    }

    display.sendBuffer();
}

// ==========================================
// Plugin Sensors Page (Phase 6)
// Fully auto-generated from SensorRegistry — adding a new sensor via
// REGISTER_SENSOR() makes it show up here with zero changes to this
// function. Shows a friendly empty state until any exist.
// ==========================================

void drawPluginsPage()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(20, 14, "PLUGINS");

    display.drawLine(0, 18, 128, 18);

    display.setFont(u8g2_font_6x12_tr);

    String lines[4];
    uint8_t lineCount = 0;
    sensorRegistry.getOledSummaryLines(lines, 4, lineCount);

    if (lineCount == 0)
    {
        display.drawStr(0, 34, "No plugin sensors");
        display.drawStr(0, 46, "registered yet.");
    }
    else
    {
        for (uint8_t i = 0; i < lineCount; i++)
        {
            display.drawStr(0, 30 + (i * 11), lines[i].c_str());
        }
    }

    display.sendBuffer();
}
// ==========================================
// Health Page (v1.0)
//
// Compact summary of HealthMonitor — heap/flash/LittleFS usage, CPU
// load estimate, WiFi/MQTT status, and restart/boot counters. Same
// visual language as drawSystemPage() (title + rule + 6x12 rows),
// just one more page in the existing rotation — no changes needed to
// any other page's layout.
// ==========================================
void drawHealthPage()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(20, 14, "HEALTH");

    display.drawLine(0, 18, 128, 18);

    display.setFont(u8g2_font_6x12_tr);

    String heapLine = "Heap: " + String(healthMonitor.getHeapUsedPercent()) + "%  FS: " + String(healthMonitor.getFsUsedPercent()) + "%";
    display.drawStr(0, 29, heapLine.c_str());

    String cpuLine = "CPU: ~" + String(healthMonitor.getCpuLoadPercent()) + "%   RSSI: " + String(healthMonitor.getRSSI());
    display.drawStr(0, 39, cpuLine.c_str());

    String netLine = "WiFi:" + wifiManager.getStateText();
    if (netLine.length() > 21) netLine = netLine.substring(0, 21); // fit 128px @ 6x12
    display.drawStr(0, 49, netLine.c_str());

    String bootLine = "Boots:" + String(healthMonitor.getBootCount()) + " Restarts:" + String(healthMonitor.getRestartCount());
    display.drawStr(0, 59, bootLine.c_str());

    // Small corner badge when a threshold is currently in a bad state —
    // the same crossing already fired a NotificationManager alert once,
    // this is just a persistent reminder while it stays true.
    if (!healthMonitor.isHealthy())
    {
        display.drawStr(104, 14, "!");
    }

    display.sendBuffer();
}

// ==========================================
// OTA Progress Screen (v1.0)
//
// Drawn directly (like showBootScreen()) instead of going through the
// normal page-rotation switch, because updateDisplay() calls this
// every loop() while OTAManager::isActive() is true, taking priority
// over page rotation and the notification overlay. Never called from
// the async upload handler itself — see the priority-lock comment in
// updateDisplay() for why that separation matters.
// ==========================================
void showOTAScreen()
{
    display.clearBuffer();

    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(14, 14, "FIRMWARE UPDATE");

    display.drawLine(0, 18, 128, 18);

    display.setFont(u8g2_font_6x12_tr);

    String status = otaManager.getStatusText();
    if (status.length() > 21) status = status.substring(0, 21);
    display.drawStr(0, 32, status.c_str());

    // Progress bar
    uint8_t pct = otaManager.getProgressPercent();
    const int barX = 4, barY = 40, barW = 120, barH = 10;
    display.drawFrame(barX, barY, barW, barH);
    int fillW = (barW - 2) * pct / 100;
    if (fillW > 0)
        display.drawBox(barX + 1, barY + 1, fillW, barH - 2);

    String pctLine = String(pct) + "%";
    display.drawStr(52, 62, pctLine.c_str());

    display.sendBuffer();
}
