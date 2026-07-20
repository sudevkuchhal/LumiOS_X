#include "system_manager.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_ota_ops.h>
#include "event_logger.h"

SystemManager systemManager;

// ==========================================
// Initialization
// ==========================================

void SystemManager::begin()
{
}



void SystemManager::update()
{
    // ==========================
    // Safe Delayed Restart
    // ==========================
    if (restartPending && millis() >= restartAt)
    {
        Serial.println("Restarting now (requested via API)...");
        delay(50);
        ESP.restart();
    }
}

// ==========================
// Restart Control
// ==========================

void SystemManager::requestRestart()
{
    restartPending = true;
    restartAt = millis() + 800; // give the HTTP response time to flush
    eventLogger.log(EventType::RESTART, "requested via API");
}

bool SystemManager::isRestartPending()
{
    return restartPending;
}
// ==========================================
// Live System Information
// ==========================================

String SystemManager::getHeap()
{
    return String(ESP.getFreeHeap() / 1024) + " KB";
}

String SystemManager::getCPU()
{
    return String(ESP.getCpuFreqMHz()) + " MHz";
}

String SystemManager::getFlash()
{
    return String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB";
}

String SystemManager::getUptime()
{
    unsigned long seconds = millis() / 1000;

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    char buffer[16];

    sprintf(buffer, "%02d:%02d:%02d",
            hours,
            minutes,
            secs);

    return String(buffer);
}

// ==========================================
// Device Information
// ==========================================

String SystemManager::getChipModel()
{
    return String(ESP.getChipModel());
}

String SystemManager::getSDKVersion()
{
    return String(ESP.getSdkVersion());
}

String SystemManager::getResetReason()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON:   return "Power-On";
        case ESP_RST_EXT:       return "External Reset";
        case ESP_RST_SW:        return "Software Restart";
        case ESP_RST_PANIC:     return "Crash / Panic";
        case ESP_RST_INT_WDT:   return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:  return "Task Watchdog";
        case ESP_RST_WDT:       return "Other Watchdog";
        case ESP_RST_DEEPSLEEP: return "Woke From Deep Sleep";
        case ESP_RST_BROWNOUT:  return "Brownout (Power Dip)";
        case ESP_RST_SDIO:      return "SDIO Reset";
        default:                return "Unknown";
    }
}

int SystemManager::getChipCores()
{
    return ESP.getChipCores();
}

int SystemManager::getChipRevision()
{
    return ESP.getChipRevision();
}