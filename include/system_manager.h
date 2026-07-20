#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>

class SystemManager
{
public:
    // ==========================
    // Initialization
    // ==========================

    void begin();
    void update();

    // ==========================
    // Live System Information
    // ==========================

    String getHeap();
    String getCPU();
    String getFlash();
    String getUptime();

    // ==========================
    // Device Information
    // ==========================

    String getChipModel();
    String getSDKVersion();
    String getResetReason();       // human-readable (e.g. "Power-On", "Software Restart")
    int getChipCores();
    int getChipRevision();

    // ==========================
    // Restart Control
    // (safe delayed restart so HTTP
    //  response can flush first)
    // ==========================

    void requestRestart();
    bool isRestartPending();

private:
    unsigned long lastUpdate = 0;

    bool restartPending = false;
    unsigned long restartAt = 0;
};

extern SystemManager systemManager;

#endif