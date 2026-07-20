#include "ota_manager.h"
#include "config.h"
#include "settings.h"
#include "event_logger.h"
#include "error_logger.h"
#include <LittleFS.h>
#include <Update.h>

OTAManager otaManager;

void OTAManager::begin()
{
    active = false;
    bytesWritten = 0;
    expectedTotal = 0;
    statusText = "Idle";
    lastError = "";
}

bool OTAManager::onUploadStart(const String &filename, size_t contentLengthHint)
{
    lastError = "";

    if (!settings.otaEnabled)
    {
        lastError = "OTA is disabled in Settings";
        errorLogger.log(ErrorCategory::OTA, lastError);
        return false;
    }

    Serial.printf("OTA Update Start: %s\n", filename.c_str());

    active = true;
    bytesWritten = 0;
    expectedTotal = contentLengthHint;
    statusText = "Uploading";

    eventLogger.log(EventType::OTA_STARTED, filename);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
        Update.printError(Serial);
        lastError = "Update.begin() failed (not enough OTA partition space?)";
        errorLogger.log(ErrorCategory::OTA, lastError);
        active = false;
        statusText = "Failed";
        return false;
    }

    return true;
}

bool OTAManager::onUploadWrite(uint8_t *data, size_t len, bool isFirstChunk)
{
    if (!active)
        return false;

    // ---- Firmware Validation ----
    // A valid ESP32 Arduino app image always starts with the 0xE9
    // magic byte (esp_image_header_t.magic). Rejecting anything else
    // immediately avoids flashing a corrupt/wrong-file upload and
    // bricking the running app partition.
    if (isFirstChunk)
    {
        if (len < 1 || data[0] != OTA_MAGIC_BYTE)
        {
            lastError = "Invalid firmware image (bad magic byte) — upload rejected";
            errorLogger.log(ErrorCategory::OTA, lastError);
            Update.abort();
            active = false;
            statusText = "Failed";
            return false;
        }
    }

    if (Update.write(data, len) != len)
    {
        Update.printError(Serial);
        lastError = "Flash write failed mid-upload";
        errorLogger.log(ErrorCategory::OTA, lastError);
        statusText = "Failed";
        return false;
    }

    bytesWritten += len;

    if (bytesWritten < OTA_MIN_FIRMWARE_BYTES && expectedTotal > 0 && expectedTotal < OTA_MIN_FIRMWARE_BYTES)
    {
        // Whole upload is suspiciously small for real firmware —
        // still let Update.end() do the final CRC/size check, but
        // flag it early in the status text so the dashboard warns
        // the user rather than looking stuck at "Uploading".
        statusText = "Uploading (warning: small file)";
    }
    else
    {
        statusText = "Uploading";
    }

    return true;
}

void OTAManager::onUploadEnd(bool updateOk, size_t totalBytes)
{
    active = false;

    if (updateOk && bytesWritten >= OTA_MIN_FIRMWARE_BYTES)
    {
        statusText = "Success - Rebooting";
        Serial.printf("OTA Update Success: %uB\n", (unsigned)totalBytes);
        eventLogger.log(EventType::OTA_FINISHED, "success, " + String(totalBytes) + "B");
        persistResult(true);
    }
    else
    {
        if (lastError.length() == 0)
            lastError = bytesWritten < OTA_MIN_FIRMWARE_BYTES
                             ? "Uploaded file too small to be valid firmware"
                             : "Update.end() reported failure";
        statusText = "Failed";
        errorLogger.log(ErrorCategory::OTA, lastError);
        eventLogger.log(EventType::OTA_FINISHED, "failed: " + lastError);
        persistResult(false);
    }
}

void OTAManager::persistResult(bool success)
{
    File f = LittleFS.open(OTA_INFO_FILE, "w");
    if (!f)
        return;
    f.printf("result=%s\n", success ? "success" : "failed");
    f.printf("uptime=%lu\n", millis() / 1000);
    f.printf("bytes=%u\n", (unsigned)bytesWritten);
    f.close();
}

bool OTAManager::isActive() { return active; }

uint8_t OTAManager::getProgressPercent()
{
    if (!active) return 0;
    if (expectedTotal == 0) return 0; // AsyncWebServer doesn't always give us Content-Length up front
    uint32_t pct = (uint32_t)(((uint64_t)bytesWritten * 100) / expectedTotal);
    return pct > 100 ? 100 : (uint8_t)pct;
}

String OTAManager::getStatusText() { return statusText; }
String OTAManager::getLastError()  { return lastError; }

String OTAManager::getLastResult()
{
    if (!LittleFS.exists(OTA_INFO_FILE))
        return "none";

    File f = LittleFS.open(OTA_INFO_FILE, "r");
    if (!f) return "none";

    String result = "none";
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("result="))
            result = line.substring(7);
    }
    f.close();
    return result;
}

String OTAManager::getLastOtaUptime()
{
    if (!LittleFS.exists(OTA_INFO_FILE))
        return "";

    File f = LittleFS.open(OTA_INFO_FILE, "r");
    if (!f) return "";

    String uptime = "";
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("uptime="))
            uptime = line.substring(7);
    }
    f.close();
    return uptime;
}

String OTAManager::getStatusJson()
{
    String json = "{";
    json += "\"active\":" + String(active ? "true" : "false") + ",";
    json += "\"progress\":" + String(getProgressPercent()) + ",";
    json += "\"status\":\"" + statusText + "\",";
    json += "\"error\":\"" + lastError + "\",";
    json += "\"lastResult\":\"" + getLastResult() + "\",";
    json += "\"lastOtaUptime\":\"" + getLastOtaUptime() + "\",";
    json += "\"otaEnabled\":" + String(settings.otaEnabled ? "true" : "false");
    json += "}";
    return json;
}
