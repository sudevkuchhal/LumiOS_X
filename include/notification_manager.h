#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <Arduino.h>

#define NOTIF_HISTORY_SIZE 15

// Notification Priority System (Phase 5).
// Defaults to INFO so every existing setNotification(msg) call site
// keeps compiling and behaving exactly as before.
enum class NotifPriority
{
    INFO,
    WARNING,
    CRITICAL
};

class NotificationManager
{
public:
    void begin();
    void update();

    // Set Notification (priority defaults to INFO — backward compatible)
    void setNotification(String message, NotifPriority priority = NotifPriority::INFO);

    // Get Current Notification
    String getCurrentNotification();
    NotifPriority getCurrentPriority();
    String getCurrentPriorityText(); // "info" / "warning" / "critical"

    // Status
    bool hasNotification();

    // Monotonic counter, incremented on every setNotification() call.
    // Consumers (OLED, etc.) can poll this cheaply to detect "there's a
    // new notification since I last checked" without re-comparing strings.
    uint32_t getNotificationSeq();

    // Clear
    void clear();

    // History (ring buffer) — returns a JSON array string
    // e.g. [{"msg":"...","uptime":123,"priority":"warning"},...] newest first
    String getHistoryJson();

    static String priorityToText(NotifPriority p);

private:
    String currentNotification = "No Alerts";
    NotifPriority currentPriority = NotifPriority::INFO;
    unsigned long notificationTime = 0;

    const unsigned long DISPLAY_TIME = 5000; // 5 sec

    // Ring buffer of past notifications
    String historyMsg[NOTIF_HISTORY_SIZE];
    unsigned long historyTime[NOTIF_HISTORY_SIZE];
    NotifPriority historyPriority[NOTIF_HISTORY_SIZE];
    uint8_t historyCount = 0;
    uint8_t historyHead = 0; // index where the NEXT entry will be written

    uint32_t notificationSeq = 0; // bumped every setNotification() call

    void pushHistory(String message, NotifPriority priority);
    static String jsonEscape(const String &in);
};

extern NotificationManager notificationManager;

#endif
