#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <Arduino.h>

// ==========================================================
// LumiOS X — Event Logger (v1.0)
//
// Sibling of ErrorLogger — same persisted, rotating, timestamped
// text-log design — but for expected lifecycle events rather than
// failures. Gives the dashboard a real audit trail: WiFi/MQTT
// connect-lost transitions, OTA start/finish, notifications, plugin
// sensors registering, settings changes, boots/restarts.
// ==========================================================

enum class EventType
{
    WIFI_CONNECTED,
    WIFI_LOST,
    MQTT_CONNECTED_EVT,
    MQTT_LOST,
    OTA_STARTED,
    OTA_FINISHED,
    NOTIFICATION_CREATED,
    SENSOR_ADDED,
    SETTINGS_CHANGED,
    BOOT,
    RESTART
};

class EventLogger
{
public:
    void begin();

    void log(EventType type, const String &detail = "");

    // JSON array, newest first, e.g.
    // [{"t":1234,"ev":"wifi_connected","detail":"..."}]
    String getJson(int maxEntries = 30);

    void clear();

    static const char *eventToText(EventType e);

private:
    static String jsonEscape(const String &in);
    void trimIfNeeded();
};

extern EventLogger eventLogger;

#endif
