#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — Error Logger (v1.0)
//
// Small, dependency-free, persisted error log. Every entry is
// timestamped (seconds since boot — this board has no RTC/NTP wall
// clock, so uptime is the honest timestamp we can actually promise)
// and tagged with a category so the dashboard can filter by subsystem.
//
// Stored as plain text in LittleFS (same convention as settings.txt /
// bootcount.txt already used by StorageManager) so it survives
// reboots and OTA updates. Capped at LOG_MAX_LINES — oldest entries
// are trimmed automatically so a stuck error condition can't fill the
// filesystem.
//
// ADDITIVE module — nothing else needs to change to adopt it; call
// errorLogger.log(...) from anywhere errors are already detected.
// ==========================================================

enum class ErrorCategory
{
    RUNTIME,
    NETWORK,
    OTA,
    SENSOR,
    STORAGE,
    DISPLAY_ERR
};

class ErrorLogger
{
public:
    void begin();

    void log(ErrorCategory category, const String &message);

    // JSON array, newest first, e.g.
    // [{"t":1234,"cat":"network","msg":"..."}]
    String getJson(int maxEntries = 30);

    unsigned long getTotalCount() const { return totalCount; }
    unsigned long getCountSince(unsigned long uptimeSec) const;

    void clear();

    static const char *categoryToText(ErrorCategory c);

private:
    unsigned long totalCount = 0;
    static String jsonEscape(const String &in);
    void trimIfNeeded();
};

extern ErrorLogger errorLogger;

#endif
