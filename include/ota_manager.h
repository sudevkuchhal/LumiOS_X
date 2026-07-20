#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — OTA Manager (v1.0)
//
// Wraps the existing /update upload handler (webserver.cpp, Stage G)
// with real progress tracking, firmware validation, a simple version
// check, and persisted OTA history — without changing the existing
// route URLs or the fact that Update.h does the actual flashing.
// webserver.cpp still owns the AsyncWebServer callbacks; it just
// calls into this class at each stage instead of talking to Update.h
// directly, so the OLED/dashboard can show live progress.
//
// "Protected OTA" = gated by the existing HTTP Basic Auth
// (settings.authEnabled/authUser/authPass, unchanged) AND the
// existing settings.otaEnabled toggle — both already existed; this
// module just also enforces otaEnabled here so a disabled-OTA device
// rejects uploads even if a request slips past the route guard.
//
// HARDWARE VALIDATION REQUIRED:
//   - Actual OTA rollback (booting the previous app partition if the
//     new firmware fails to boot) depends on esp_ota_mark_app_valid
//     / app rollback being exercised on real hardware; this firmware
//     does not currently call esp_ota_mark_app_invalid_and_reboot(),
//     so a bad flash currently just boots into broken firmware rather
//     than auto-reverting. Documented in ARCHITECTURE.md.
//   - Confirm the board's partition table actually has two OTA app
//     slots (ota_0/ota_1) for its real flash size before relying on
//     this in the field.
// ==========================================================

class OTAManager
{
public:
    void begin();

    // ---- Called from webserver.cpp's /update handlers ----

    // Returns false (and fills lastError) if the upload should be
    // rejected before Update.begin() is ever called.
    bool onUploadStart(const String &filename, size_t contentLengthHint);

    // Feeds each chunk to Update.h; validates the very first bytes
    // (ESP32 app image magic byte) before committing to the flash.
    bool onUploadWrite(uint8_t *data, size_t len, bool isFirstChunk);

    void onUploadEnd(bool updateOk, size_t totalBytes);

    // ---- Status (dashboard / OLED) ----
    bool   isActive();
    uint8_t getProgressPercent();
    String getStatusText();
    String getLastError();

    // Persisted across reboots
    String getLastResult();     // "success" / "failed" / "none"
    String getLastOtaUptime();  // uptime (seconds) at last completed OTA, as string

    String getStatusJson();

private:
    bool active = false;
    size_t bytesWritten = 0;
    size_t expectedTotal = 0;
    String statusText = "Idle";
    String lastError = "";

    void persistResult(bool success);
};

extern OTAManager otaManager;

#endif
