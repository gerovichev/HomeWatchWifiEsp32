#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t dataMutex;

/**
 * RAII guard for dataMutex, the lock covering state shared between the display
 * core (loop()) and the network core (dataUpdateTask).
 *
 * The rule this type exists to enforce: hold the lock only long enough to copy
 * values in or out. No sensor reads, no HTTP, no String formatting, no
 * drawString() inside the critical section - those belong outside it, working
 * from a snapshot. Locking around slow work is what previously let a blocking
 * DHT22 read on the display core stall the network task.
 *
 * A null dataMutex (before setup() creates it) is treated as "no contention
 * yet", so early-boot callers work without special-casing.
 */
class DataLock {
public:
  explicit DataLock(TickType_t waitTicks = portMAX_DELAY) : held(false) {
    if (dataMutex != NULL) {
      held = (xSemaphoreTake(dataMutex, waitTicks) == pdTRUE);
    }
  }

  ~DataLock() {
    if (held) {
      xSemaphoreGive(dataMutex);
    }
  }

  DataLock(const DataLock &) = delete;
  DataLock &operator=(const DataLock &) = delete;

  // False only when a bounded wait timed out; callers using the default
  // portMAX_DELAY can ignore this.
  bool acquired() const { return held || dataMutex == NULL; }

private:
  bool held;
};
