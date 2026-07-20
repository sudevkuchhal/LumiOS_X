#include "sensor_registry.h"
#include "notification_manager.h"
#include "event_logger.h"

SensorRegistry &SensorRegistry::instance()
{
    static SensorRegistry inst;
    return inst;
}

bool SensorRegistry::registerSensor(SensorBase *sensor)
{
    if (!sensor || sensorCount >= SENSOR_REGISTRY_MAX)
        return false;

    sensors[sensorCount] = sensor;
    lastAlertLevel[sensorCount] = SensorAlertLevel::NONE;
    sensorCount++;
    return true;
}

void SensorRegistry::beginAll()
{
    Serial.println();
    Serial.println("========== SENSOR PLUGINS ==========");

    if (sensorCount == 0)
    {
        Serial.println("No plugin sensors registered.");
    }
    else
    {
        for (uint8_t i = 0; i < sensorCount; i++)
        {
            sensors[i]->begin();
            Serial.print("Registered: ");
            Serial.println(sensors[i]->getDisplayName());
            eventLogger.log(EventType::SENSOR_ADDED, sensors[i]->getDisplayName());
        }
    }

    Serial.println("=====================================");
}

void SensorRegistry::updateAll()
{
    for (uint8_t i = 0; i < sensorCount; i++)
    {
        SensorBase *s = sensors[i];

        if (!s->isEnabled())
            continue;

        s->update();

        SensorAlertLevel level = s->getAlertLevel();

        // Edge-triggered: only fire a notification the moment the level
        // changes into something active, same pattern main.cpp already
        // uses for the built-in DHT/HC-SR04/MQ2 alerts (wasDistanceWarning
        // etc.) — so a plugin sensor doesn't spam a notification every
        // single loop() tick while it stays in warning/critical.
        if (level != lastAlertLevel[i] && level != SensorAlertLevel::NONE)
        {
            NotifPriority p = (level == SensorAlertLevel::CRITICAL)
                                   ? NotifPriority::CRITICAL
                                   : NotifPriority::WARNING;
            notificationManager.setNotification(s->getAlertMessage(), p);
        }

        lastAlertLevel[i] = level;
    }
}

SensorBase *SensorRegistry::get(uint8_t index) const
{
    if (index >= sensorCount)
        return nullptr;
    return sensors[index];
}

SensorBase *SensorRegistry::find(const String &id) const
{
    for (uint8_t i = 0; i < sensorCount; i++)
    {
        if (id == sensors[i]->getId())
            return sensors[i];
    }
    return nullptr;
}

String SensorRegistry::getAllJson() const
{
    String json = "[";

    for (uint8_t i = 0; i < sensorCount; i++)
    {
        SensorBase *s = sensors[i];

        if (i) json += ",";
        json += "{";
        json += "\"id\":\"" + String(s->getId()) + "\",";
        json += "\"name\":\"" + String(s->getDisplayName()) + "\",";
        json += "\"enabled\":" + String(s->isEnabled() ? "true" : "false") + ",";

        String alertText = "none";
        if (s->getAlertLevel() == SensorAlertLevel::CRITICAL) alertText = "critical";
        else if (s->getAlertLevel() == SensorAlertLevel::WARNING) alertText = "warning";
        json += "\"alert\":\"" + alertText + "\",";

        json += s->getJsonFields();
        json += "}";
    }

    json += "]";
    return json;
}

void SensorRegistry::getOledSummaryLines(String lines[], uint8_t maxLines, uint8_t &outCount) const
{
    outCount = 0;

    for (uint8_t i = 0; i < sensorCount && outCount < maxLines; i++)
    {
        if (!sensors[i]->isEnabled())
            continue;

        lines[outCount++] = sensors[i]->getOledSummary();
    }
}
