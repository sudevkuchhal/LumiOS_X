#ifndef CLOUD_RELAY_H
#define CLOUD_RELAY_H

#include <Arduino.h>

// ==========================================
// LumiOS X - Cloud Relay (Stage I.2 — hardened)
//
// Purpose: let the phone view live data and control LEDs
// from ANYWHERE — home WiFi, office WiFi, or mobile data —
// WITHOUT port forwarding, a static IP, or a VPS.
//
// How it works:
//   ESP32  --outbound-->  MQTT Broker (internet)  <--outbound-- Phone (browser)
//
// Both the ESP32 and the phone's dashboard only ever make
// OUTBOUND connections to the broker. Neither one needs to
// be reachable from outside, so there's nothing to forward
// and no firewall/NAT issue — this is the same trick smart
// bulbs and IoT apps use.
//
// Non-blocking: never calls delay(). If WiFi drops or the
// broker connection drops, it retries on its own on the
// next update() call, with exponential backoff.
//
// Stage I.2 additions (on top of the original Stage I):
//   - Last-Will-Testament -> /avail topic ("online"/"offline", retained)
//   - Periodic /heartbeat  -> lets the dashboard detect a stalled
//                             device even if the broker link is fine
//   - Retained /status publishes -> fresh subscribers see the last
//                             known state immediately, no 3s wait
//   - Command acknowledgement -> /ack echoes back the command id so
//                             the dashboard knows a command landed
//   - JSON string escaping -> a quote/backslash in a notification or
//                             IP string can no longer corrupt the
//                             payload and silently break the dashboard
//   - Reconnect backoff + structured [CloudRelay] log lines
// ==========================================

class CloudRelay
{
public:
    void begin();
    void update();      // call every loop()
    bool isConnected();

    // v1.0 — Health Monitor / dashboard reconnect indicator.
    // Human-readable current state ("Connected", "Reconnecting...",
    // "Offline (no WiFi)") and how many consecutive reconnect
    // failures we've seen since the last successful connect (resets
    // to 0 on connect).
    String getStateText();
    unsigned int getReconnectFailCount();

private:
    unsigned long lastPublish = 0;
    unsigned long lastHeartbeat = 0;
    unsigned long lastReconnectAttempt = 0;
    unsigned long reconnectBackoffMs = 0;
    unsigned int  reconnectFailCount = 0;
    unsigned long heartbeatSeq = 0;

    void reconnect();
    void publishStatus();
    void publishHeartbeat();
    void publishAck(const String &id, const String &cmd, bool ok);
    void handleCommand(String rawPayload);

    static String jsonEscape(const String &in);

    static void onMessage(char *topic, byte *payload, unsigned int length);
};

extern CloudRelay cloudRelay;

#endif
