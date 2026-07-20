#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <DHT.h>

class SensorManager
{
public:
    void begin();
    void update();

    // --- Temperature / Humidity (DHT11) ---
    float getTemperature();
    float getHumidity();

    // --- Distance (HC-SR04, cm) ---
    float getDistance();
    bool isDistanceOnline();
    bool isDistanceWarning();   // object too close

    // --- Smoke / Gas (MQ2, raw ADC 0-4095) ---
    int getSmokeRaw();
    int getSmokePercent();      // 0-100 scaled for UI gauges
    bool isSmokeWarning();
    bool isSmokeDanger();

    // --- Overall ---
    String getStatus();         // "Online" / "Offline" (DHT)
    bool hasActiveAlert();      // true if any sensor is in warning/danger

private:
    float temperature = 0.0;
    float humidity = 0.0;
    String status = "Offline";

    float distanceCm = -1.0;
    bool distanceSensorOnline = false;

    int smokeRaw = 0;

    unsigned long lastDistanceRead = 0;
    unsigned long lastSmokeRead = 0;

    float readDistanceRaw();
};

extern SensorManager sensorManager;

#endif
