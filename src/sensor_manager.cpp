#include "sensor_manager.h"
#include "config.h"

DHT dht(DHT_PIN, DHT_TYPE);

SensorManager sensorManager;

// ==========================================
// Initialization
// ==========================================

void SensorManager::begin()
{
    dht.begin();
    status = "Online";

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    pinMode(MQ2_PIN, INPUT);
}

// ==========================================
// Update — called from loop()
// ==========================================

void SensorManager::update()
{
    // --- DHT11 Temperature/Humidity ---
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t))
        temperature = t - 1.0;

    if (!isnan(h))
        humidity = h + 5.0;

    // --- HC-SR04 Distance (throttled: sensor needs ~60ms between reads) ---
    if (millis() - lastDistanceRead >= 200)
    {
        lastDistanceRead = millis();

        float d = readDistanceRaw();

        if (d > 0 && d < DISTANCE_MAX_CM)
        {
            distanceCm = d;
            distanceSensorOnline = true;
        }
        else
        {
            // no echo received -> either nothing in range or sensor missing
            distanceSensorOnline = false;
        }
    }

    // --- MQ2 Smoke/Gas (analog, cheap to read every loop) ---
    if (millis() - lastSmokeRead >= 200)
    {
        lastSmokeRead = millis();
        smokeRaw = analogRead(MQ2_PIN);
    }
}

// ==========================================
// HC-SR04 raw pulse read -> centimeters
// ==========================================

float SensorManager::readDistanceRaw()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 25ms timeout ≈ ~4.3m max range, avoids blocking loop() for long
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000UL);

    if (duration == 0)
        return -1.0; // timeout / no object / sensor not connected

    return (duration * 0.0343f) / 2.0f;
}

// ==========================================
// Getters — Temperature / Humidity
// ==========================================

float SensorManager::getTemperature()
{
    return temperature;
}

float SensorManager::getHumidity()
{
    return humidity;
}

String SensorManager::getStatus()
{
    return status;
}

// ==========================================
// Getters — Distance
// ==========================================

float SensorManager::getDistance()
{
    return distanceCm;
}

bool SensorManager::isDistanceOnline()
{
    return distanceSensorOnline;
}

bool SensorManager::isDistanceWarning()
{
    return distanceSensorOnline && distanceCm <= DISTANCE_WARN_CM;
}

// ==========================================
// Getters — Smoke / Gas
// ==========================================

int SensorManager::getSmokeRaw()
{
    return smokeRaw;
}

int SensorManager::getSmokePercent()
{
    int pct = (smokeRaw * 100) / 4095;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

bool SensorManager::isSmokeWarning()
{
    return smokeRaw >= SMOKE_WARN_RAW && smokeRaw < SMOKE_DANGER_RAW;
}

bool SensorManager::isSmokeDanger()
{
    return smokeRaw >= SMOKE_DANGER_RAW;
}

// ==========================================
// Overall
// ==========================================

bool SensorManager::hasActiveAlert()
{
    return isDistanceWarning() || isSmokeWarning() || isSmokeDanger();
}
