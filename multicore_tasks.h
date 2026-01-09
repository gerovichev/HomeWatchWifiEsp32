#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Task priorities (higher number = higher priority)
#define DISPLAY_TASK_PRIORITY    2  // Display updates - high priority
#define DATA_UPDATE_TASK_PRIORITY 1 // Data fetching - medium priority
#define SENSOR_TASK_PRIORITY     1  // Sensor reading - medium priority
#define MQTT_TASK_PRIORITY       1  // MQTT - medium priority

// Task stack sizes
#define DISPLAY_TASK_STACK       4096
#define DATA_UPDATE_TASK_STACK   8192  // Larger for HTTP operations
#define SENSOR_TASK_STACK        2048
#define MQTT_TASK_STACK          4096

// Core assignments
#define CORE_0 0  // Main loop, display updates
#define CORE_1 1  // Background tasks (HTTP, sensors, MQTT)

// Detect number of CPU cores
// ESP32-C3 has only one core, ESP32 has two cores
#if defined(CONFIG_FREERTOS_UNICORE) || defined(ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3)
  #define NUM_CORES 1
  #define USE_SINGLE_CORE 1
#else
  #define NUM_CORES 2
  #define USE_SINGLE_CORE 0
#endif

// Task handles
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t dataUpdateTaskHandle;
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t mqttTaskHandle;

// Semaphores for thread-safe access
extern SemaphoreHandle_t displayMutex;
extern SemaphoreHandle_t dataMutex;

// Flags for task coordination (declared in multicore_tasks.cpp)
extern volatile bool dataUpdateRequested;
extern volatile bool sensorReadRequested;

// Function declarations
void initMulticoreTasks();
void displayTask(void* parameter);
void dataUpdateTask(void* parameter);
void sensorTask(void* parameter);
void mqttTask(void* parameter);

