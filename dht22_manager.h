#pragma once

#include <DHT_U.h>
#include "global_config.h"

// Pin and sensor type definitions
#define DHTPIN 12      // Pin which is connected to the DHT sensor.
#define DHTTYPE DHT22  // DHT 22 (AM2302)

/**
 * Owns the DHT22 and caches its last good reading.
 *
 * Composition rather than `: public DHT_Unified` - the sensor is an
 * implementation detail, and inheriting published the whole DHT_Unified API to
 * every caller.
 *
 * Threading: refresh() performs the blocking sensor I/O and is called from
 * dataUpdateTask (core 0). The print* methods run on the display core and only
 * read the cached values under dataMutex, so a slow sensor can no longer stall
 * the display or the network task.
 */
class Dht22_manager
{
public:
  Dht22_manager();

  void dht22Start();   // One-time hardware init plus a first reading
  void refresh();      // Blocking sensor read; call from the data-update task

  void printHomeTemp();
  void printHumidity();

  // NAN when no successful reading has been taken yet.
  float getHomeTemp() const;

private:
  DHT_Unified dht;

  float homeTemp = NAN;
  float homeHumidity = NAN;
  uint8_t sensorErrorCount = 0;

  void initSensor();
  void printSensorDetails(const sensor_t& sensor, const char* type);
  void handleSensorError(const __FlashStringHelper* what);
};
