#include "notification_manager.h"
#include "event_logger.h"

NotificationManager notificationManager;

// ==========================================
// Initialization
// ==========================================

void NotificationManager::begin()
{
    currentNotification = "No Alerts";
    currentPriority = NotifPriority::INFO;
    notificationTime = 0;
    historyCount = 0;
    historyHead = 0;
}

// ==========================================
// Update
// ==========================================

void NotificationManager::update()
{
    if (currentNotification != "No Alerts")
    {
        if (millis() - notificationTime >= DISPLAY_TIME)
        {
            clear();
        }
    }
}

// ==========================================
// Set Notification
// ==========================================

void NotificationManager::setNotification(String message, NotifPriority priority)
{
    // Avoid spamming identical repeats into history back-to-back
    bool isDuplicate = (historyCount > 0 &&
        historyMsg[(historyHead + NOTIF_HISTORY_SIZE - 1) % NOTIF_HISTORY_SIZE] == message);

    currentNotification = message;
    currentPriority = priority;
    notificationTime = millis();
    notificationSeq++;

    if (!isDuplicate)
    {
        pushHistory(message, priority);
        eventLogger.log(EventType::NOTIFICATION_CREATED, priorityToText(priority) + ": " + message);
    }
}

// ==========================================
// History Ring Buffer
// ==========================================

void NotificationManager::pushHistory(String message, NotifPriority priority)
{
    historyMsg[historyHead] = message;
    historyTime[historyHead] = millis();
    historyPriority[historyHead] = priority;

    historyHead = (historyHead + 1) % NOTIF_HISTORY_SIZE;

    if (historyCount < NOTIF_HISTORY_SIZE)
        historyCount++;
}

String NotificationManager::priorityToText(NotifPriority p)
{
    switch (p)
    {
        case NotifPriority::CRITICAL: return "critical";
        case NotifPriority::WARNING:  return "warning";
        default:                      return "info";
    }
}

String NotificationManager::jsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 4);

    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

String NotificationManager::getHistoryJson()
{
    String json = "[";

    // Walk backwards from the most recently written entry (newest first)
    for (uint8_t i = 0; i < historyCount; i++)
    {
        uint8_t idx = (historyHead + NOTIF_HISTORY_SIZE - 1 - i) % NOTIF_HISTORY_SIZE;

        if (i) json += ",";
        json += "{";
        json += "\"msg\":\"" + jsonEscape(historyMsg[idx]) + "\",";
        json += "\"uptime\":" + String(historyTime[idx] / 1000) + ",";
        json += "\"priority\":\"" + priorityToText(historyPriority[idx]) + "\"";
        json += "}";
    }

    json += "]";
    return json;
}

// ==========================================
// Clear Notification
// ==========================================

void NotificationManager::clear()
{
    currentNotification = "No Alerts";
    currentPriority = NotifPriority::INFO;
    notificationTime = millis();
}

// ==========================================
// Getters
// ==========================================

String NotificationManager::getCurrentNotification()
{
    return currentNotification;
}

bool NotificationManager::hasNotification()
{
    return currentNotification != "No Alerts";
}

uint32_t NotificationManager::getNotificationSeq()
{
    return notificationSeq;
}

NotifPriority NotificationManager::getCurrentPriority()
{
    return currentPriority;
}

String NotificationManager::getCurrentPriorityText()
{
    return priorityToText(currentPriority);
}
