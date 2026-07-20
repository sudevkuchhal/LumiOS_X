#include "wifi_manager.h"
#include "wifi_config.h"
#include "storage_manager.h"
#include "notification_manager.h"
#include "config.h"
#include "event_logger.h"
#include "error_logger.h"

WiFiManagerX wifiManager;

// ==========================================
// begin()
// Non-blocking: just loads saved credentials
// and kicks off the first attempt. Actual
// connecting happens across future update()
// calls, so setup() never stalls here.
// ==========================================
void WiFiManagerX::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // we drive reconnects ourselves

    hasSaved = storageManager.loadWiFi(savedSSID, savedPassword);

    if (hasSaved)
    {
        Serial.println("Saved WiFi found, will try first");
    }
    else
    {
        Serial.println("No saved WiFi, will try fallback list");
    }

    state = WiFiConnState::IDLE;
    attemptStart = 0; // forces update() to kick off an attempt immediately
}

// ==========================================
// Attempt helpers
// ==========================================
void WiFiManagerX::beginSavedAttempt()
{
    Serial.print("Trying saved WiFi: ");
    Serial.println(savedSSID);

    WiFi.disconnect(true);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    state = WiFiConnState::TRYING_SAVED;
    attemptStart = millis();
}

void WiFiManagerX::beginListAttempt(int index)
{
    Serial.print("Trying: ");
    Serial.println(wifiList[index].ssid);

    WiFi.disconnect(true);
    WiFi.begin(wifiList[index].ssid, wifiList[index].password);

    listIndex = index;
    state = WiFiConnState::TRYING_LIST;
    attemptStart = millis();
}

// Called when the current attempt has timed out without connecting.
// Decides what to try next.
void WiFiManagerX::advanceAfterTimeout()
{
    if (state == WiFiConnState::TRYING_SAVED)
    {
        // Saved WiFi failed -> move to fallback list
        if (WIFI_COUNT > 0)
        {
            beginListAttempt(0);
        }
        else
        {
            failedCycles++;
            cooldownDurationMs = min(WIFI_COOLDOWN_BASE_MS * (1UL << min(failedCycles - 1, 4U)), WIFI_COOLDOWN_MAX_MS);
            state = WiFiConnState::COOLDOWN;
            cooldownStart = millis();
        }
        return;
    }

    if (state == WiFiConnState::TRYING_LIST)
    {
        int next = listIndex + 1;

        if (next < WIFI_COUNT)
        {
            beginListAttempt(next);
        }
        else
        {
            failedCycles++;
            cooldownDurationMs = min(WIFI_COOLDOWN_BASE_MS * (1UL << min(failedCycles - 1, 4U)), WIFI_COOLDOWN_MAX_MS);
            Serial.println("All WiFi attempts failed, cooling down for " + String(cooldownDurationMs / 1000) + "s...");
            errorLogger.log(ErrorCategory::NETWORK, "All WiFi attempts failed (cycle " + String(failedCycles) + ")");
            state = WiFiConnState::COOLDOWN;
            cooldownStart = millis();
        }
        return;
    }
}

// ==========================================
// update()
// Call every loop(). Never blocks.
// ==========================================
void WiFiManagerX::update()
{
    switch (state)
    {
        case WiFiConnState::IDLE:
        {
            if (hasSaved)
                beginSavedAttempt();
            else if (WIFI_COUNT > 0)
                beginListAttempt(0);
            else
            {
                state = WiFiConnState::COOLDOWN;
                cooldownStart = millis();
            }
            break;
        }

        case WiFiConnState::TRYING_SAVED:
        case WiFiConnState::TRYING_LIST:
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                state = WiFiConnState::CONNECTED;
                wasConnected = true;
                failedCycles = 0; // v1.0 — successful connect resets exponential backoff

                // If we connected via the fallback list, remember it
                // for next boot / next reconnect cycle.
                if (listIndex != -1)
                {
                    storageManager.saveWiFi(wifiList[listIndex].ssid,
                                             wifiList[listIndex].password);
                    listIndex = -1;
                }

                Serial.println("================================");
                Serial.println("WiFi Connected");
                Serial.print("SSID : "); Serial.println(getSSID());
                Serial.print("IP   : "); Serial.println(getIP());
                Serial.println("================================");

                if (!everConnected)
                {
                    everConnected = true;
                    notificationManager.setNotification("WiFi Connected: " + getSSID());
                }
                else
                {
                    notificationManager.setNotification("WiFi Reconnected");
                }
                eventLogger.log(EventType::WIFI_CONNECTED, getSSID() + " (" + getIP() + ")");
            }
            else if (millis() - attemptStart >= CONNECT_TIMEOUT)
            {
                advanceAfterTimeout();
            }
            break;
        }

        case WiFiConnState::CONNECTED:
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                wasConnected = false;
                notificationManager.setNotification("WiFi Lost - Reconnecting...");
                eventLogger.log(EventType::WIFI_LOST);
                state = WiFiConnState::IDLE; // restart the attempt cycle
            }
            break;
        }

        case WiFiConnState::COOLDOWN:
        {
            if (millis() - cooldownStart >= cooldownDurationMs)
            {
                state = WiFiConnState::IDLE;
            }
            break;
        }
    }
}

// Force an immediate fresh attempt cycle (used by /wifi/save after
// new credentials are stored, so the device doesn't wait for the
// current cooldown/timeout to expire).
bool WiFiManagerX::reconnect()
{
    hasSaved = storageManager.loadWiFi(savedSSID, savedPassword);
    listIndex = -1;
    failedCycles = 0;
    state = WiFiConnState::IDLE;
    return true;
}

bool WiFiManagerX::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManagerX::getIP()
{
    return WiFi.localIP().toString();
}

String WiFiManagerX::getSSID()
{
    return WiFi.SSID();
}

int WiFiManagerX::getRSSI()
{
    return WiFi.RSSI();
}

String WiFiManagerX::getStateText()
{
    switch (state)
    {
        case WiFiConnState::IDLE:         return "Idle";
        case WiFiConnState::TRYING_SAVED: return "Connecting (saved)";
        case WiFiConnState::TRYING_LIST:  return "Connecting (fallback)";
        case WiFiConnState::CONNECTED:    return "Connected";
        case WiFiConnState::COOLDOWN:     return "Retrying in " + String(getNextRetryMs() / 1000) + "s";
        default:                          return "Unknown";
    }
}

// ==========================================
// v1.0 — WiFi Recovery status getters
// ==========================================
unsigned int WiFiManagerX::getReconnectAttempts()
{
    return failedCycles;
}

unsigned long WiFiManagerX::getNextRetryMs()
{
    if (state != WiFiConnState::COOLDOWN)
        return 0;

    unsigned long elapsed = millis() - cooldownStart;
    if (elapsed >= cooldownDurationMs)
        return 0;

    return cooldownDurationMs - elapsed;
}
