#include "error_logger.h"
#include "config.h"
#include <LittleFS.h>

ErrorLogger errorLogger;

const char *ErrorLogger::categoryToText(ErrorCategory c)
{
    switch (c)
    {
        case ErrorCategory::RUNTIME: return "runtime";
        case ErrorCategory::NETWORK: return "network";
        case ErrorCategory::OTA:     return "ota";
        case ErrorCategory::SENSOR:  return "sensor";
        case ErrorCategory::STORAGE: return "storage";
        case ErrorCategory::DISPLAY_ERR: return "display";
        default:                     return "unknown";
    }
}

String ErrorLogger::jsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 4);
    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        if (c == '"' || c == '\\') out += '\\';
        else if (c == '\n' || c == '\r' || c == '|') continue; // '|' is our field separator
        out += c;
    }
    return out;
}

void ErrorLogger::begin()
{
    // Count existing lines so getTotalCount()/uptime dashboards are
    // accurate immediately after boot, not just for entries logged
    // this session.
    totalCount = 0;
    if (LittleFS.exists(ERROR_LOG_FILE))
    {
        File f = LittleFS.open(ERROR_LOG_FILE, "r");
        if (f)
        {
            while (f.available())
            {
                f.readStringUntil('\n');
                totalCount++;
            }
            f.close();
        }
    }
}

void ErrorLogger::log(ErrorCategory category, const String &message)
{
    totalCount++;

    String line = String(millis() / 1000) + "|" + categoryToText(category) + "|" + jsonEscape(message);

    Serial.println("[ErrorLogger][" + String(categoryToText(category)) + "] " + message);

    File f = LittleFS.open(ERROR_LOG_FILE, "a");
    if (f)
    {
        f.println(line);
        f.close();
    }

    trimIfNeeded();
}

// Keeps the log file bounded. Reads the whole file (small — capped at
// LOG_MAX_LINES lines of short text) and rewrites it with only the
// newest LOG_MAX_LINES entries. Only runs when the cap is exceeded,
// so normal logging stays a cheap append.
//
// Bug fix (v1.0 QA pass): the previous version kept LOG_MAX_LINES + 1
// entries instead of LOG_MAX_LINES (off-by-one), and its "keep" step
// wrote out the FRONT of the scratch buffer rather than the tail, so
// once the log reached steady state it wasn't actually dropping the
// oldest line — it was re-writing the same window back unchanged on
// every single log() call, which is wasted flash wear, not "only
// runs when the cap is exceeded" as the comment above promises.
// Rewritten as a straightforward single-pass ring buffer: `lines[]`
// holds the last LOG_MAX_LINES entries seen, `head` always points at
// the oldest surviving one once the file has wrapped.
void ErrorLogger::trimIfNeeded()
{
    if (!LittleFS.exists(ERROR_LOG_FILE))
        return;

    File f = LittleFS.open(ERROR_LOG_FILE, "r");
    if (!f)
        return;

    String lines[LOG_MAX_LINES];
    int count = 0; // total non-empty lines seen
    int head = 0;  // ring-buffer write cursor

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
        return; // already within the cap — nothing to rewrite

    File out = LittleFS.open(ERROR_LOG_FILE, "w");
    if (!out)
        return;

    // The buffer wrapped, so `head` now points at the oldest
    // surviving entry (the next slot that would have been
    // overwritten). Walk forward LOG_MAX_LINES times to emit
    // oldest -> newest.
    for (int i = 0; i < LOG_MAX_LINES; i++)
        out.println(lines[(head + i) % LOG_MAX_LINES]);

    out.close();
}

unsigned long ErrorLogger::getCountSince(unsigned long uptimeSec) const
{
    // Lightweight scan — only used by health summaries, not hot path.
    unsigned long n = 0;
    if (!LittleFS.exists(ERROR_LOG_FILE))
        return 0;

    File f = LittleFS.open(ERROR_LOG_FILE, "r");
    if (!f)
        return 0;

    while (f.available())
    {
        String l = f.readStringUntil('\n');
        int sep = l.indexOf('|');
        if (sep > 0 && (unsigned long)l.substring(0, sep).toInt() >= uptimeSec)
            n++;
    }
    f.close();
    return n;
}

String ErrorLogger::getJson(int maxEntries)
{
    if (!LittleFS.exists(ERROR_LOG_FILE))
        return "[]";

    File f = LittleFS.open(ERROR_LOG_FILE, "r");
    if (!f)
        return "[]";

    // Read all lines, then emit the newest `maxEntries` in reverse order.
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
    int start = count - 1;
    int emitted = 0;
    for (int i = start; i >= 0 && emitted < maxEntries; i--, emitted++)
    {
        String l = allLines[i];
        int p1 = l.indexOf('|');
        int p2 = l.indexOf('|', p1 + 1);
        if (p1 < 0 || p2 < 0) continue;

        String t = l.substring(0, p1);
        String cat = l.substring(p1 + 1, p2);
        String msg = l.substring(p2 + 1);

        if (emitted) json += ",";
        json += "{\"t\":" + t + ",\"cat\":\"" + cat + "\",\"msg\":\"" + msg + "\"}";
    }
    json += "]";
    return json;
}

void ErrorLogger::clear()
{
    if (LittleFS.exists(ERROR_LOG_FILE))
        LittleFS.remove(ERROR_LOG_FILE);
    totalCount = 0;
}
