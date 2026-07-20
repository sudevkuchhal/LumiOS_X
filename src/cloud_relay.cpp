#include "cloud_relay.h"

#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "wifi_manager.h"
#include "sensor_manager.h"
#include "notification_manager.h"
#include "leds.h"
#include "event_logger.h"
#include "error_logger.h"

CloudRelay cloudRelay;

// Own TCP socket + MQTT client, kept private to this file so nothing
// else in the project needs to know PubSubClient exists.
static WiFiClient relayNetClient;
static PubSubClient mqtt(relayNetClient);
static bool mqttWasConnected = false;

// ==========================================
// Small logging helper — every line from this module is tagged
// so it's easy to grep the serial monitor for cloud relay activity
// vs. the rest of the firmware's logs.
// ==========================================
static void logInfo(const String &msg)  { Serial.println("[CloudRelay][INFO]  " + msg); }
static void logWarn(const String &msg)  { Serial.println("[CloudRelay][WARN]  " + msg); }
static void logError(const String &msg) { Serial.println("[CloudRelay][ERROR] " + msg); }

// Human-readable meaning of PubSubClient's connection state codes,
// used so failed-connect logs are actually actionable instead of
// just printing a bare integer.
static const char *mqttStateText(int state)
{
    switch (state)
    {
        case -4: return "MQTT_CONNECTION_TIMEOUT (broker didn't respond in time)";
        case -3: return "MQTT_CONNECTION_LOST (TCP link dropped)";
        case -2: return "MQTT_CONNECT_FAILED (couldn't open TCP socket to broker)";
        case -1: return "MQTT_DISCONNECTED";
        case  0: return "MQTT_CONNECTED";
        case  1: return "MQTT_CONNECT_BAD_PROTOCOL";
        case  2: return "MQTT_CONNECT_BAD_CLIENT_ID";
        case  3: return "MQTT_CONNECT_UNAVAILABLE (broker refused, try again later)";
        case  4: return "MQTT_CONNECT_BAD_CREDENTIALS";
        case  5: return "MQTT_CONNECT_UNAUTHORIZED";
        default: return "UNKNOWN_STATE";
    }
}

// ==========================================
// JSON string escaping
//
// BUG FIXED: publishStatus() used to splice WiFi.localIP() and the
// live notification text directly into a hand-built JSON string. Any
// '"' or '\' in that text (e.g. a notification generated from
// user-editable settings) produced invalid JSON. The dashboard's
// JSON.parse() call is wrapped in try/catch that silently drops the
// message, so from that point on EVERY future status update looked
// like "the dashboard never receives live data" even though messages
// were arriving over MQTT just fine. This mirrors the jsonEscape()
// already used in notification_manager.cpp, applied everywhere a
// dynamic string is embedded here too.
// ==========================================
String CloudRelay::jsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 4);

    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        if (c == '"' || c == '\\')
            out += '\\';
        else if (c == '\n' || c == '\r')
            continue; // strip stray newlines rather than break the JSON
        out += c;
    }
    return out;
}

// ==========================================
// Setup
// ==========================================
void CloudRelay::begin()
{
    Serial.println();
    Serial.println("========== CLOUD RELAY ==========");
    Serial.print("Broker      : ");
    Serial.println(MQTT_BROKER);
    Serial.print("Device ID   : ");
    Serial.println(IOT_DEVICE_ID);
    Serial.print("Status topic: ");
    Serial.println(MQTT_TOPIC_STATUS);
    Serial.print("Cmd topic   : ");
    Serial.println(MQTT_TOPIC_CMD);
    Serial.print("Avail topic : ");
    Serial.println(MQTT_TOPIC_AVAIL);
    Serial.print("Heartbeat   : ");
    Serial.println(MQTT_TOPIC_HEARTBEAT);
    Serial.print("Ack topic   : ");
    Serial.println(MQTT_TOPIC_ACK);
    Serial.println("==================================");

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt.setBufferSize(1024); // default 256B is too small for our status JSON
    mqtt.setKeepAlive(30);    // seconds; generous enough to survive brief loop() stalls
    mqtt.setSocketTimeout(5); // seconds; don't let a dead TCP link hang the whole loop

    reconnectBackoffMs = MQTT_RECONNECT_INTERVAL_MS;
}

bool CloudRelay::isConnected()
{
    return mqtt.connected();
}

// ==========================================
// Connection handling (non-blocking, with backoff)
// ==========================================
void CloudRelay::reconnect()
{
    if (millis() - lastReconnectAttempt < reconnectBackoffMs)
        return;

    lastReconnectAttempt = millis();

    logInfo("Attempting broker connection... (attempt " + String(reconnectFailCount + 1) + ")");

    // Unique-ish client id so multiple boots/devices don't collide on the broker
    String clientId = String(IOT_DEVICE_ID) + "-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    // Last-Will-Testament: if this connection dies WITHOUT a clean
    // disconnect (crash, brownout, WiFi drop), the broker publishes
    // "offline" (retained) to /avail on our behalf. This is what lets
    // the dashboard reliably show "Device Offline" instead of frozen
    // stale data when the ESP32 itself is the one that's gone.
    bool ok = mqtt.connect(
        clientId.c_str(),
        nullptr, nullptr,             // no username/password on the public test broker
        MQTT_TOPIC_AVAIL, 1, true, "offline",
        true                          // clean session
    );

    if (ok)
    {
        logInfo("Connected to broker as " + clientId);

        mqtt.subscribe(MQTT_TOPIC_CMD, 1);

        // Tell the world we're back, immediately — retained, so anyone
        // (re)connecting a moment later sees "online" without waiting.
        mqtt.publish(MQTT_TOPIC_AVAIL, "online", true);

        // Push a status + heartbeat right away so a phone opening the
        // dashboard right after this reconnect doesn't sit staring at
        // "--" for up to MQTT_PUBLISH_INTERVAL_MS.
        publishStatus();
        publishHeartbeat();

        reconnectFailCount = 0;
        reconnectBackoffMs = MQTT_RECONNECT_INTERVAL_MS;

        if (!mqttWasConnected)
        {
            eventLogger.log(EventType::MQTT_CONNECTED_EVT, clientId);
            mqttWasConnected = true;
        }
    }
    else
    {
        int state = mqtt.state();
        logError("Connect failed, rc=" + String(state) + " -> " + mqttStateText(state));
        errorLogger.log(ErrorCategory::NETWORK, "MQTT connect failed: " + String(mqttStateText(state)));

        reconnectFailCount++;
        // Exponential backoff so a broker outage doesn't spam reconnect
        // attempts forever; capped so we still recover reasonably fast
        // once the broker/network comes back.
        reconnectBackoffMs = min(
            (unsigned long)MQTT_RECONNECT_INTERVAL_MS * (1UL << min(reconnectFailCount, 6U)),
            (unsigned long)MQTT_RECONNECT_MAX_BACKOFF_MS
        );
        logWarn("Next attempt in " + String(reconnectBackoffMs / 1000) + "s");
    }
}

// ==========================================
// Main loop hook
// ==========================================
void CloudRelay::update()
{
    // No point touching MQTT if we don't even have WiFi/internet yet
    if (!wifiManager.isConnected())
        return;

    if (!mqtt.connected())
    {
        if (mqttWasConnected)
        {
            eventLogger.log(EventType::MQTT_LOST);
            mqttWasConnected = false;
        }
        reconnect();
        return;
    }

    mqtt.loop(); // services incoming messages + keepalive pings

    unsigned long now = millis();

    if (now - lastPublish >= MQTT_PUBLISH_INTERVAL_MS)
    {
        lastPublish = now;
        publishStatus();
    }

    if (now - lastHeartbeat >= MQTT_HEARTBEAT_INTERVAL_MS)
    {
        lastHeartbeat = now;
        publishHeartbeat();
    }
}

// ==========================================
// Publish live status (mirrors /info from webserver.cpp,
// trimmed down to what the remote dashboard actually shows)
// Published RETAINED so a phone that connects mid-session sees the
// last known state immediately instead of waiting for the next tick.
// ==========================================
void CloudRelay::publishStatus()
{
    String json = "{";

    json += "\"ip\":\"" + jsonEscape(WiFi.localIP().toString()) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";

    json += "\"temperature\":" + String(sensorManager.getTemperature(), 1) + ",";
    json += "\"humidity\":" + String(sensorManager.getHumidity(), 1) + ",";

    json += "\"distance\":" + String(sensorManager.isDistanceOnline() ? sensorManager.getDistance() : -1, 1) + ",";
    json += "\"distanceWarning\":" + String(sensorManager.isDistanceWarning() ? "true" : "false") + ",";

    json += "\"smoke\":" + String(sensorManager.getSmokePercent()) + ",";
    json += "\"smokeWarning\":" + String(sensorManager.isSmokeWarning() ? "true" : "false") + ",";
    json += "\"smokeDanger\":" + String(sensorManager.isSmokeDanger() ? "true" : "false") + ",";

    json += "\"led1\":" + String(leds.state(1) ? "true" : "false") + ",";
    json += "\"led2\":" + String(leds.state(2) ? "true" : "false") + ",";
    json += "\"led3\":" + String(leds.state(3) ? "true" : "false") + ",";
    json += "\"led4\":" + String(leds.state(4) ? "true" : "false") + ",";
    json += "\"led5\":" + String(leds.state(5) ? "true" : "false") + ",";

    json += "\"notification\":\"" + jsonEscape(notificationManager.getCurrentNotification()) + "\",";
    json += "\"uptime\":" + String(millis() / 1000);

    json += "}";

    bool ok = mqtt.publish(MQTT_TOPIC_STATUS, json.c_str(), true /* retain */);
    if (!ok)
        logError("publishStatus() failed (payload " + String(json.length()) + "B, rc=" + String(mqtt.state()) + ")");
}

// ==========================================
// Heartbeat — small, NOT retained. The dashboard treats "no heartbeat
// for N seconds" as the offline signal, independent of whether the
// browser's own broker connection still looks alive.
// ==========================================
void CloudRelay::publishHeartbeat()
{
    heartbeatSeq++;

    String json = "{";
    json += "\"seq\":" + String(heartbeatSeq) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";

    bool ok = mqtt.publish(MQTT_TOPIC_HEARTBEAT, json.c_str(), false /* not retained */);
    if (!ok)
        logError("publishHeartbeat() failed, rc=" + String(mqtt.state()));
}

// ==========================================
// Command acknowledgement — echoes the command id back so the
// dashboard can confirm the ESP32 actually received and executed it,
// instead of just assuming client.publish() on the browser side
// means the device got it.
// ==========================================
void CloudRelay::publishAck(const String &id, const String &cmd, bool ok)
{
    String json = "{";
    json += "\"id\":\"" + jsonEscape(id) + "\",";
    json += "\"cmd\":\"" + jsonEscape(cmd) + "\",";
    json += "\"ok\":" + String(ok ? "true" : "false") + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";

    bool sent = mqtt.publish(MQTT_TOPIC_ACK, json.c_str(), false);
    if (!sent)
        logError("publishAck() failed, rc=" + String(mqtt.state()));
}

// ==========================================
// Incoming command handling
// ==========================================
void CloudRelay::onMessage(char *topic, byte *payload, unsigned int length)
{
    String raw;
    raw.reserve(length);
    for (unsigned int i = 0; i < length; i++)
        raw += (char)payload[i];

    cloudRelay.handleCommand(raw);
}

// Payload format: "<id>|<cmd>", e.g. "a3f9c1|led1:on"
// Backward compatible: a payload with no "|" is treated as a bare
// command (no ack id) so older clients / manual `mosquitto_pub` tests
// still work exactly as before.
void CloudRelay::handleCommand(String rawPayload)
{
    rawPayload.trim();

    String id;
    String cmd;

    int sep = rawPayload.indexOf('|');
    if (sep >= 0)
    {
        id = rawPayload.substring(0, sep);
        cmd = rawPayload.substring(sep + 1);
    }
    else
    {
        cmd = rawPayload;
    }
    cmd.trim();

    logInfo("Command received -> \"" + cmd + "\"" + (id.length() ? (" [id=" + id + "]") : ""));

    bool known = true;

    if      (cmd == "led1:on")  leds.on(1);
    else if (cmd == "led1:off") leds.off(1);
    else if (cmd == "led2:on")  leds.on(2);
    else if (cmd == "led2:off") leds.off(2);
    else if (cmd == "led3:on")  leds.on(3);
    else if (cmd == "led3:off") leds.off(3);
    else if (cmd == "led4:on")  leds.on(4);
    else if (cmd == "led4:off") leds.off(4);
    else if (cmd == "led5:on")  leds.on(5);
    else if (cmd == "led5:off") leds.off(5);
    else if (cmd == "led:all:on")  leds.allOn();
    else if (cmd == "led:all:off") leds.allOff();
    else
    {
        known = false;
        logWarn("Unknown command, ignored: \"" + cmd + "\"");
    }

    // Ack every command that carried an id, whether or not it was
    // recognized, so the dashboard never sits waiting on an ack that's
    // never coming.
    if (id.length())
        publishAck(id, cmd, known);

    // Publish immediately so the dashboard reflects the change
    // without waiting for the next scheduled interval.
    publishStatus();
}

// ==========================================
// v1.0 — Health Monitor / dashboard reconnect indicator getters
// ==========================================
String CloudRelay::getStateText()
{
    if (!wifiManager.isConnected())
        return "Offline (no WiFi)";
    if (mqtt.connected())
        return "Connected";
    if (reconnectFailCount > 0)
        return "Reconnecting... (attempt " + String(reconnectFailCount + 1) + ")";
    return "Connecting...";
}

unsigned int CloudRelay::getReconnectFailCount()
{
    return reconnectFailCount;
}
