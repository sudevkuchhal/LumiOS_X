#include "leds.h"
#include "webserver.h"

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "sensor_manager.h"
#include "notification_manager.h"
#include "settings.h"
#include "display.h"
#include "system_manager.h"
#include "storage_manager.h"
#include "wifi_manager.h"
#include "developer_api.h"
#include "config.h"
#include "sensor_registry.h"
#include "version_manager.h"
#include "health_monitor.h"
#include "error_logger.h"
#include "event_logger.h"
#include "ota_manager.h"

AsyncWebServer server(80);
WebServerX webServer;

// ==========================================
// Auth Guard (Stage H)
// When settings.authEnabled is true, sensitive
// control routes require HTTP Basic Auth. Returns
// true if the request is allowed to proceed; if it
// returns false it has already sent a 401 response.
// ==========================================
static bool requireAuth(AsyncWebServerRequest *request)
{
    if (!settings.authEnabled)
        return true;

    if (!request->authenticate(settings.authUser.c_str(), settings.authPass.c_str()))
    {
        request->requestAuthentication();
        return false;
    }

    return true;
}

void WebServerX::begin()
{
    Serial.println();
    Serial.println("========== WEB SERVER ==========");

    // LittleFS is already mounted by StorageManager

    Serial.println("LittleFS Ready");

    // NOTE: We intentionally do NOT wait for or require WiFi to be
    // connected here. AsyncWebServer's begin() only starts the TCP
    // listener — it works fine even if WiFi connects a few seconds
    // later (which it now does, since WiFi connection is non-blocking).
    // The old code returned early here if WiFi wasn't connected yet,
    // which meant the server would NEVER start for that boot cycle,
    // even after WiFi came online later.

    Serial.println("Creating Routes...");

    // ==========================
    // Dashboard
    // ==========================
    server.serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html");

    // ==========================
    // Ping
    // ==========================
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "text/plain", "PONG");
    });

    // ==========================
    // System Information
    // ==========================
    
server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request)
{
    String json = "{";

    // =========================
    // Project
    // =========================
    json += "\"project\":\"LumiOS X\",";
    json += "\"version\":\"1.0.0\",";
    json += "\"status\":\"ONLINE\",";

    // =========================
    // WiFi
    // =========================
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";

    // =========================
    // ESP32
    // =========================
    json += "\"cpu\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"flash\":" + String(ESP.getFlashChipSize()/1024/1024) + ",";
    json += "\"uptime\":" + String(millis()/1000) + ",";

    // =========================
    // Sensor
    // =========================
    json += "\"temperature\":" + String(sensorManager.getTemperature(),1) + ",";
    json += "\"humidity\":" + String(sensorManager.getHumidity(),1) + ",";

    // Distance (HC-SR04)
    json += "\"distance\":" + String(sensorManager.isDistanceOnline() ? sensorManager.getDistance() : -1, 1) + ",";
    json += "\"distanceOnline\":" + String(sensorManager.isDistanceOnline() ? "true" : "false") + ",";
    json += "\"distanceWarning\":" + String(sensorManager.isDistanceWarning() ? "true" : "false") + ",";

    // Smoke / Gas (MQ2)
    json += "\"smoke\":" + String(sensorManager.getSmokePercent()) + ",";
    json += "\"smokeRaw\":" + String(sensorManager.getSmokeRaw()) + ",";
    json += "\"smokeWarning\":" + String(sensorManager.isSmokeWarning() ? "true" : "false") + ",";
    json += "\"smokeDanger\":" + String(sensorManager.isSmokeDanger() ? "true" : "false") + ",";

    // =========================
    // LED Status
    // =========================
json += "\"led1\":" + String(leds.state(1)) + ",";
json += "\"led2\":" + String(leds.state(2)) + ",";
json += "\"led3\":" + String(leds.state(3)) + ",";
json += "\"led4\":" + String(leds.state(4)) + ",";
json += "\"led5\":" + String(leds.state(5)) + ",";

    // LED Automation Engine (Phase 2)
    json += "\"ledAutoMode\":\"" + leds.getAutoModeText() + "\",";
    json += "\"ledOverride1\":" + String(leds.isOverridden(1) ? "true" : "false") + ",";
    json += "\"ledOverride2\":" + String(leds.isOverridden(2) ? "true" : "false") + ",";
    json += "\"ledOverride3\":" + String(leds.isOverridden(3) ? "true" : "false") + ",";
    json += "\"ledOverride4\":" + String(leds.isOverridden(4) ? "true" : "false") + ",";
    json += "\"ledOverride5\":" + String(leds.isOverridden(5) ? "true" : "false") + ",";

    // =========================
    // OLED
    // =========================
    json+="\"oledPage\":\""+getCurrentPageName()+"\",";

    // =========================
    // Settings
    // =========================
    json += "\"theme\":\"" + settings.getTheme() + "\",";
    json += "\"developerMode\":\"" + settings.getDeveloperMode() + "\",";
    json += "\"autoPage\":\"" + settings.getAutoPage() + "\",";
    json += "\"animation\":\"" + settings.getAnimation() + "\",";
    json += "\"ota\":\"" + settings.getOTA() + "\",";
    json += "\"brightness\":" + settings.getBrightness() + ",";
    json += "\"language\":\"" + settings.getLanguage() + "\",";
    json += "\"authEnabled\":\"" + settings.getAuth() + "\",";

    // =========================
    // System (Stage G)
    // =========================
    json += "\"bootCount\":" + String(storageManager.getBootCount()) + ",";
    json += "\"resetReason\":\"" + systemManager.getResetReason() + "\",";
    json += "\"chipModel\":\"" + systemManager.getChipModel() + "\",";
    json += "\"chipCores\":" + String(systemManager.getChipCores()) + ",";
    json += "\"chipRevision\":" + String(systemManager.getChipRevision()) + ",";
    json += "\"sdkVersion\":\"" + systemManager.getSDKVersion() + "\",";

    // =========================
    // Developer Info
    // =========================
    json += "\"devName\":\"" + developerAPI.getName() + "\",";
    json += "\"devRole\":\"" + developerAPI.getRole() + "\",";
    json += "\"devGitHub\":\"" + developerAPI.getGitHub() + "\",";
    json += "\"devLinkedIn\":\"" + developerAPI.getLinkedIn() + "\",";
    json += "\"devRepos\":" + String(developerAPI.getRepos()) + ",";
    json += "\"devStars\":" + String(developerAPI.getStars()) + ",";
    json += "\"devStatus\":\"" + developerAPI.getStatus() + "\",";

    // =========================
    // Notification
    // =========================
    json += "\"notification\":\"" +
            notificationManager.getCurrentNotification() +
            "\",";
    json += "\"notifPriority\":\"" + notificationManager.getCurrentPriorityText() + "\"";

    json += "}";

    request->send(200, "application/json", json);
});
    // ==========================
    // API Status
    // ==========================
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String json = "{";

        json += "\"project\":\"LumiOS X\",";
        json += "\"version\":\"1.0.0\",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI());

        json += "}";

        request->send(200, "application/json", json);
    });

    // Note: /api/version, /api/health, /api/logs/*, and /api/ota/status
    // are registered together further down (right before the 404
    // handler) so they sit next to each other for readability.

    // ==========================
    // LED APIs
    // ==========================

    // Stage H fix: these 15 routes previously had no requireAuth() check

    // at all, even though /led/all/on|off and /led/auto/toggle did — so
    // turning auth ON in settings did not actually stop anyone from
    // driving individual LEDs. Every route below now checks requireAuth()
    // first, same as the "all" variants already did.

    server.on("/led/1/on", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.on(1);
        request->send(200, "text/plain", "LED1 ON");
    });

    server.on("/led/1/off", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.off(1);
        request->send(200, "text/plain", "LED1 OFF");
    });

    server.on("/led/2/on", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.on(2);
        request->send(200, "text/plain", "LED2 ON");
    });

    server.on("/led/2/off", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.off(2);
        request->send(200, "text/plain", "LED2 OFF");
    });

    server.on("/led/3/on", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.on(3);
        request->send(200, "text/plain", "LED3 ON");
    });

    server.on("/led/3/off", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.off(3);
        request->send(200, "text/plain", "LED3 OFF");
    });

    server.on("/led/4/on", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.on(4);
        request->send(200, "text/plain", "LED4 ON");
    });

    server.on("/led/4/off", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.off(4);
        request->send(200, "text/plain", "LED4 OFF");
    });

    server.on("/led/5/on", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.on(5);
        request->send(200, "text/plain", "LED5 ON");
    });

    server.on("/led/5/off", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        leds.off(5);
        request->send(200, "text/plain", "LED5 OFF");
    });
// ==========================
// Toggle APIs
// ==========================

server.on("/led/1/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.toggle(1);
    request->send(200,"text/plain","OK");
});

server.on("/led/2/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.toggle(2);
    request->send(200,"text/plain","OK");
});

server.on("/led/3/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.toggle(3);
    request->send(200,"text/plain","OK");
});

server.on("/led/4/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.toggle(4);
    request->send(200,"text/plain","OK");
});

server.on("/led/5/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.toggle(5);
    request->send(200,"text/plain","OK");
});

// ==========================
// All LEDs
// ==========================

server.on("/led/all/on", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.allOn();
    notificationManager.setNotification("All LEDs turned ON");
    request->send(200, "text/plain", "ALL LEDS ON");
});

server.on("/led/all/off", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.allOff();
    notificationManager.setNotification("All LEDs turned OFF");
    request->send(200, "text/plain", "ALL LEDS OFF");
});

// ==========================
// LED Automation Engine (Phase 2)
// ==========================

server.on("/led/auto/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    leds.setAutoMode(!leds.isAutoMode());
    notificationManager.setNotification("LED Automation: " + leds.getAutoModeText());
    request->send(200, "text/plain", leds.getAutoModeText());
});

// ==========================
// Restart API
// ==========================

server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    request->send(200, "text/plain", "Restarting...");
    notificationManager.setNotification("Device Restarting...", NotifPriority::WARNING);
    systemManager.requestRestart();
});

// ==========================
// Settings APIs
// ==========================

server.on("/settings/developer/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    settings.toggleDeveloperMode();
    notificationManager.setNotification("Developer Mode: " + settings.getDeveloperMode());
    request->send(200, "text/plain", settings.getDeveloperMode());
});

server.on("/settings/animation/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    settings.toggleAnimation();
    request->send(200, "text/plain", settings.getAnimation());
});

server.on("/settings/autopage/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    settings.toggleAutoPage();
    request->send(200, "text/plain", settings.getAutoPage());
});

server.on("/settings/theme", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    if (!request->hasParam("value"))
    {
        request->send(400, "text/plain", "Missing 'value' param");
        return;
    }
    settings.theme = request->getParam("value")->value();
    settings.save(); // theme is set directly here rather than via a toggle*() method, so persist explicitly
    notificationManager.setNotification("Theme changed to " + settings.theme);
    request->send(200, "text/plain", "Theme set to " + settings.theme);
});

// ==========================
// WiFi Tools
// ==========================

server.on("/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request)
{
    // v1.0 fix: this route was missing the requireAuth() guard that
    // /wifi/save and every other WiFi/LED/OTA control route already
    // has (see the Stage H comment above the LED routes — "sensitive
    // control routes (LED/restart/wifi/OTA) require HTTP Basic Auth").
    // A network scan itself is low-risk, but it's still WiFi manager
    // functionality and should follow the same gate as the rest of
    // that section for consistency.
    if (!requireAuth(request)) return;

    int n = WiFi.scanNetworks();
    String json = "[";

    for (int i = 0; i < n; i++)
    {
        if (i) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
        json += "}";
    }

    json += "]";
    WiFi.scanDelete();

    request->send(200, "application/json", json);
});

server.on("/wifi/save", HTTP_GET, [](AsyncWebServerRequest *request)
{
    if (!requireAuth(request)) return;
    if (!request->hasParam("ssid") || !request->hasParam("password"))
    {
        request->send(400, "text/plain", "Missing 'ssid' or 'password' param");
        return;
    }

    String ssid = request->getParam("ssid")->value();
    String pass = request->getParam("password")->value();

    storageManager.saveWiFi(ssid, pass);
    notificationManager.setNotification("New WiFi saved, connecting...");
    wifiManager.reconnect();

    request->send(200, "text/plain", "Saved. Connecting to new network...");
});

    // ==========================
    // Notification History (Stage E)
    // ==========================
    server.on("/notifications/history", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", notificationManager.getHistoryJson());
    });

    server.on("/notifications/clear", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        notificationManager.clear();
        request->send(200, "text/plain", "Cleared");
    });

    // ==========================
    // Plugin Sensors (Phase 6)
    // Auto-generated JSON for whatever sensors are currently
    // registered via REGISTER_SENSOR() — "[]" until any exist.
    // ==========================
    server.on("/api/plugins", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", sensorRegistry.getAllJson());
    });

    server.on("/api/plugins/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;

        if (!request->hasParam("id"))
        {
            request->send(400, "text/plain", "Missing 'id' param");
            return;
        }

        SensorBase *s = sensorRegistry.find(request->getParam("id")->value());
        if (!s)
        {
            request->send(404, "text/plain", "Unknown plugin sensor id");
            return;
        }

        s->setEnabled(!s->isEnabled());
        notificationManager.setNotification(
            String(s->getDisplayName()) + (s->isEnabled() ? ": enabled" : ": disabled"));

        request->send(200, "text/plain", s->isEnabled() ? "enabled" : "disabled");
    });

    // ==========================
    // Auth Settings (Stage H)
    // ==========================
    server.on("/settings/auth/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        settings.toggleAuth();
        notificationManager.setNotification("Dashboard Auth: " + settings.getAuth());
        request->send(200, "text/plain", settings.getAuth());
    });

    server.on("/settings/auth/credentials", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;

        if (!request->hasParam("user") || !request->hasParam("pass"))
        {
            request->send(400, "text/plain", "Missing 'user' or 'pass' param");
            return;
        }

        String user = request->getParam("user")->value();
        String pass = request->getParam("pass")->value();

        if (!settings.setCredentials(user, pass))
        {
            // Bug fix (v1.0 QA pass): SettingsManager::setCredentials()
            // enforces an 8-character minimum (raised from 4 during the
            // Stage H hardening pass), but this message was never
            // updated to match, so a rejected request told the caller
            // the wrong requirement.
            request->send(400, "text/plain", "Password must be at least 8 characters");
            return;
        }

        notificationManager.setNotification("Dashboard credentials updated");
        request->send(200, "text/plain", "Credentials updated");
    });

    // ==========================
    // OTA Firmware Update (Stage G, upgraded for v1.0)
    // Browse to /update for a simple upload form, or POST a .bin file
    // to the same URL (both unchanged from Stage G — dashboard/tools
    // that already used these URLs keep working). Now backed by
    // OTAManager for progress tracking, firmware validation (magic
    // byte check), Protected OTA (settings.otaEnabled + auth), and
    // persisted OTA history, instead of talking to Update.h directly.
    // ==========================
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;

        String page =
            "<!DOCTYPE html><html><head><title>LumiOS X OTA Update</title>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>body{font-family:sans-serif;background:#05070c;color:#eef2f7;"
            "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
            ".box{background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.1);"
            "border-radius:16px;padding:32px;max-width:420px;text-align:center}"
            "h1{color:#00e5ff;font-size:20px}input{margin:16px 0}"
            "button{background:#00e5ff;color:#05070c;border:none;padding:10px 20px;"
            "border-radius:8px;font-weight:bold;cursor:pointer}"
            "#bar{width:100%;height:10px;background:rgba(255,255,255,.1);border-radius:6px;"
            "overflow:hidden;margin-top:14px;display:none}"
            "#fill{height:100%;width:0%;background:#00e5ff;transition:width .2s}"
            "#status{opacity:.75;font-size:12px;margin-top:8px}</style></head><body>"
            "<div class='box'><h1>&#9889; LumiOS X &mdash; Firmware Update</h1>"
            "<p>Select a compiled .bin firmware image to flash over WiFi.</p>"
            "<p style='opacity:.6;font-size:11px'>Current version: " FIRMWARE_VERSION "</p>"
            "<form id='f'>"
            "<input type='file' id='fw' name='firmware' accept='.bin'><br>"
            "<button type='submit'>Upload &amp; Flash</button></form>"
            "<div id='bar'><div id='fill'></div></div>"
            "<div id='status'>Device will restart automatically once the upload completes.</div>"
            "</div>"
            "<script>"
            "document.getElementById('f').addEventListener('submit',function(e){"
            "e.preventDefault();"
            "var file=document.getElementById('fw').files[0];"
            "if(!file){return;}"
            "var xhr=new XMLHttpRequest();"
            "var bar=document.getElementById('bar');"
            "var fill=document.getElementById('fill');"
            "var status=document.getElementById('status');"
            "bar.style.display='block';"
            "xhr.upload.addEventListener('progress',function(ev){"
            "if(ev.lengthComputable){var pct=Math.round(ev.loaded/ev.total*100);"
            "fill.style.width=pct+'%';status.textContent='Uploading... '+pct+'%';}"
            "});"
            "xhr.onload=function(){status.textContent=xhr.responseText;};"
            "xhr.onerror=function(){status.textContent='Upload failed (connection lost).';};"
            "var fd=new FormData();fd.append('firmware',file);"
            "xhr.open('POST','/update');xhr.send(fd);"
            "});"
            "</script></body></html>";

        request->send(200, "text/html", page);
    });

    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            if (!requireAuth(request)) return;

            bool ok = otaManager.getStatusText() == "Success - Rebooting";
            String msg = ok ? "Update OK. Rebooting..." : ("Update FAILED: " + otaManager.getLastError());

            AsyncWebServerResponse *response = request->beginResponse(
                200, "text/plain", msg);
            response->addHeader("Connection", "close");
            request->send(response);

            if (ok)
            {
                notificationManager.setNotification("Firmware updated, restarting...", NotifPriority::WARNING);
                systemManager.requestRestart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (!index)
            {
                if (!requireAuth(request)) return;
                otaManager.onUploadStart(filename, request->contentLength());
            }

            otaManager.onUploadWrite(data, len, index == 0);

            if (final)
            {
                size_t total = index + len;
                bool ok = otaManager.isActive() && Update.end(true);
                otaManager.onUploadEnd(ok, total);
            }
        });

    // ==========================
    // API: Version Manager (v1.0)
    // ==========================
    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", versionManager.getJson());
    });

    // ==========================
    // API: Health Monitor (v1.0)
    // ==========================
    server.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", healthMonitor.getJson());
    });

    // ==========================
    // API: OTA Status (v1.0) — polled by the dashboard OTA card and
    // by the inline /update progress page as a fallback.
    // ==========================
    server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", otaManager.getStatusJson());
    });

    // ==========================
    // API: Error / Event Logs (v1.0)
    // ?limit=N optional, default 30, capped at LOG_MAX_LINES
    // ==========================
    server.on("/api/logs/errors", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        int limit = 30;
        if (request->hasParam("limit"))
            limit = request->getParam("limit")->value().toInt();
        if (limit <= 0 || limit > LOG_MAX_LINES) limit = 30;
        request->send(200, "application/json", errorLogger.getJson(limit));
    });

    server.on("/api/logs/events", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        int limit = 30;
        if (request->hasParam("limit"))
            limit = request->getParam("limit")->value().toInt();
        if (limit <= 0 || limit > LOG_MAX_LINES) limit = 30;
        request->send(200, "application/json", eventLogger.getJson(limit));
    });

    server.on("/api/logs/clear", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!requireAuth(request)) return;
        errorLogger.clear();
        eventLogger.clear();
        request->send(200, "text/plain", "Logs cleared");
    });

    // ==========================
    // 404 handler (so unknown API
    // calls don't hang silently)
    // ==========================
    server.onNotFound([](AsyncWebServerRequest *request)
    {
        request->send(404, "text/plain", "Not Found");
    });

    // ==========================
    // START THE SERVER
    // (THIS WAS THE MAIN BUG — routes
    //  were registered but the server
    //  never actually started listening
    //  on port 80. That's why the page
    //  loaded once but /info, /ping,
    //  /led/*, etc never responded.)
    // ==========================
    server.begin();
    Serial.println("Web Server Started on port 80");
}