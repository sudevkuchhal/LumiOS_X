#include "event_logger.h"
#include "config.h"
#include <LittleFS.h>

EventLogger eventLogger;

const char *EventLogger::eventToText(EventType e)
{
    switch (e)
    {
        case EventType::WIFI_CONNECTED:       return "wifi_connected";
        case EventType::WIFI_LOST:            return "wifi_lost";
        case EventType::MQTT_CONNECTED_EVT:   return "mqtt_connected";
        case EventType::MQTT_LOST:            return "mqtt_lost";
        case EventType::OTA_STARTED:          return "ota_started";
        case EventType::OTA_FINISHED:         return "ota_finished";
        case EventType::NOTIFICATION_CREATED: return "notification_created";
        case EventType::SENSOR_ADDED:         return "sensor_added";
        case EventType::SETTINGS_CHANGED:     return "settings_changed";
        case EventType::BOOT:                 return "boot";
        case EventType::RESTART:              return "restart";
        default:                              return "unknown";
    }
}

String EventLogger::jsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 4);
    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        if (c == '"' || c == '\\') out += '\\';
        else if (c == '\n' || c == '\r' || c == '|') continue;
        out += c;
    }
    return out;
}

void EventLogger::begin()
{
    // Nothing to preload — file grows via log(); trimmed automatically.
}

void EventLogger::log(EventType type, const String &detail)
{
    String line = String(millis() / 1000) + "|" + eventToText(type) + "|" + jsonEscape(detail);

    Serial.println("[EventLogger][" + String(eventToText(type)) + "] " + detail);

    File f = LittleFS.open(EVENT_LOG_FILE, "a");
    if (f)
    {
        f.println(line);
        f.close();
    }

    trimIfNeeded();
}

// Bug fix (v1.0 QA pass): see the matching fix + explanation in
// ErrorLogger::trimIfNeeded() (error_logger.cpp) — this was the same
// off-by-one / wrong-entries-kept bug, same fix (single-pass ring
// buffer capped at exactly LOG_MAX_LINES, oldest entry dropped first).
void EventLogger::trimIfNeeded()
{
    if (!LittleFS.exists(EVENT_LOG_FILE))
        return;

    File f = LittleFS.open(EVENT_LOG_FILE, "r");
    if (!f)
        return;

    String lines[LOG_MAX_LINES];
    int count = 0;
    int head = 0;

    while (f.available())
    {
        String l = f.readStringUntil('\n');
        l.trim();
        if (l.length() == 0)
            continue;

        lines[head] = l;
        head = (head + 1) % LOG_MAX_LINES;
        count++;
    }
    f.close();

    if (count <= LOG_MAX_LINES)
        return;

    File out = LittleFS.open(EVENT_LOG_FILE, "w");
    if (!out)
        return;

    for (int i = 0; i < LOG_MAX_LINES; i++)
        out.println(lines[(head + i) % LOG_MAX_LINES]);

    out.close();
}

String EventLogger::getJson(int maxEntries)
{
    if (!LittleFS.exists(EVENT_LOG_FILE))
        return "[]";

    File f = LittleFS.open(EVENT_LOG_FILE, "r");
    if (!f)
        return "[]";

    String allLines[LOG_MAX_LINES + 1];
    int count = 0;
    while (f.available() && count <= LOG_MAX_LINES)
    {
        String l = f.readStringUntil('\n');
        l.trim();
        if (l.length())
            allLines[count++] = l;
    }
    f.close();

    String json = "[";
    int emitted = 0;
    for (int i = count - 1; i >= 0 && emitted < maxEntries; i--, emitted++)
    {
        String l = allLines[i];
        int p1 = l.indexOf('|');
        int p2 = l.indexOf('|', p1 + 1);
        if (p1 < 0 || p2 < 0) continue;

        String t = l.substring(0, p1);
        String ev = l.substring(p1 + 1, p2);
        String detail = l.substring(p2 + 1);

        if (emitted) json += ",";
        json += "{\"t\":" + t + ",\"ev\":\"" + ev + "\",\"detail\":\"" + detail + "\"}";
    }
    json += "]";
    return json;
}

void EventLogger::clear()
{
    if (LittleFS.exists(EVENT_LOG_FILE))
        LittleFS.remove(EVENT_LOG_FILE);
}
