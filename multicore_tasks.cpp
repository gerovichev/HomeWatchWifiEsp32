#include "multicore_tasks.h"
#include "main_process.h"
#include "clock.h"
#include "led_display.h"
#include "weather_manager.h"
#include "currency_manager.h"
#include "dht22_manager.h"
#include "TimeManager.h"
#include "location_manager.h"
#include "MQTTClient.h"
#include "OTAUpdate.h"
#include "logger.h"
#include "constants.h"
#include "device_state.h"
#include <WiFi.h>
#include <TimeLib.h>

// Task handles
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t dataUpdateTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;

// Semaphores
SemaphoreHandle_t displayMutex = NULL;
SemaphoreHandle_t dataMutex = NULL;

// Flags
volatile bool dataUpdateRequested = false;
volatile bool sensorReadRequested = false;

// Display task - handles display updates
void displayTask(void* parameter) {
  #if USE_SINGLE_CORE
    LOG_INFO_F("Display task started (single core)");
  #else
    LOG_INFO_F("Display task started on Core 0");
  #endif
  
  while (true) {
    // Check minute change (this should be called frequently)
    Clock::getInstance().checkMinuteChange();
    
    // Always call clock_loop() - it checks its own timing and animation conditions
    clock_loop();
    
    // Always update display animation (non-blocking)
    realDisplayText();
    
    // Small delay to prevent CPU hogging and allow other tasks
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Data update task - handles HTTP requests
void dataUpdateTask(void* parameter) {
  #if USE_SINGLE_CORE
    LOG_INFO_F("Data update task started (single core)");
  #else
    LOG_INFO_F("Data update task started on Core 1");
  #endif
  
  TickType_t lastUpdateTime = 0;
  TickType_t lastOTACheck = 0;
  TickType_t lastLocationUpdate = 0;
  const TickType_t updateInterval = pdMS_TO_TICKS(Timing::DATA_UPDATE_INTERVAL_SEC * 1000);
  const TickType_t otaInterval = pdMS_TO_TICKS(3600000); // 1 hour
  const TickType_t locationInterval = pdMS_TO_TICKS(3600000); // 1 hour
  
  // Initial data load - wait a bit for WiFi to stabilize, then load data immediately
  vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds for system to stabilize
  bool initialLoadDone = false;
  
  while (true) {
    TickType_t currentTime = xTaskGetTickCount();
    bool shouldUpdate = dataUpdateRequested || 
                       (currentTime - lastUpdateTime >= updateInterval) ||
                       !initialLoadDone; // Force initial load
    
    if (shouldUpdate && WiFi.status() == WL_CONNECTED) {
      if (!initialLoadDone) {
        initialLoadDone = true;
        LOG_INFO_F("Performing initial data load...");
      }
      dataUpdateRequested = false;
      lastUpdateTime = currentTime;
      
      LOG_INFO_F("Starting data update cycle (Core 1)...");
      
      // Take mutex for thread-safe operations
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        // Update location (if needed)
        if (currentTime - lastLocationUpdate >= locationInterval) {
          location_init();
          lastLocationUpdate = currentTime;
        }
        
        // Update NTP time
        timeClient.update();
        timeNow = timeClient.getEpochTime();
        setTime(timeNow);
        
        // Update timezone
        getTimezone();
        
        // Fetch weather data
        LOG_INFO_F("Fetching weather data (Core 1)...");
        Clock::getInstance().getWeatherManager().readWeather();
        LOG_INFO_F("Weather data updated");
        
        // Fetch currency data
        LOG_INFO_F("Fetching currency rates (Core 1)...");
        Clock::getInstance().getCurrencyManager().initialize();
        LOG_INFO_F("Currency rates updated");
        
        // Adjust display intensity
        setIntensityByTime(timeNow);
        
        xSemaphoreGive(dataMutex);
        LOG_INFO_F("Data update cycle completed (Core 1)");
        LOG_INFO("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
      }
    } else if (shouldUpdate) {
      dataUpdateRequested = false;
      LOG_WARNING_F("WiFi not connected, skipping data update");
    }
    
    // Check for OTA updates (less frequently)
    if (DeviceState::getInstance().isOtaRequired() && 
        (currentTime - lastOTACheck >= otaInterval)) {
      lastOTACheck = currentTime;
      update_ota();
    }
    
    // Sleep for a while before next check
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Sensor task - handles DHT22 readings
void sensorTask(void* parameter) {
  #if USE_SINGLE_CORE
    LOG_INFO_F("Sensor task started (single core)");
  #else
    LOG_INFO_F("Sensor task started on Core 1");
  #endif
  
  const TickType_t sensorInterval = pdMS_TO_TICKS(2000); // Read every 2 seconds
  TickType_t lastWakeTime = xTaskGetTickCount();
  
  while (true) {
    if (IS_DHT_CONNECTED) {
      // Read sensor data
      Dht22_manager &dht22 = Clock::getInstance().getDht22();
      
      // These are quick operations, no need for mutex
      dht22.printHomeTemp();
      vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between readings
      dht22.printHumidity();
    }
    
    // Wait for next interval
    vTaskDelayUntil(&lastWakeTime, sensorInterval);
  }
}

// MQTT task - handles MQTT operations
void mqttTask(void* parameter) {
  #if USE_SINGLE_CORE
    LOG_INFO_F("MQTT task started (single core)");
  #else
    LOG_INFO_F("MQTT task started on Core 1");
  #endif
  
  while (true) {
    if (DeviceState::getInstance().isMqttEnabled() && WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        LOG_WARNING_F("MQTT disconnected, reconnecting...");
        reconnect();
      }
      
      // Keep MQTT client running
      client.loop();
      
      // Publish temperature periodically
      static unsigned long lastPublish = 0;
      if (millis() - lastPublish > 60000) { // Every minute
        publish_temperature();
        lastPublish = millis();
      }
    }
    
    // Check MQTT every second
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Initialize multicore tasks
void initMulticoreTasks() {
  LOG_INFO_F("Initializing FreeRTOS tasks...");
  
  // Create mutexes
  displayMutex = xSemaphoreCreateMutex();
  dataMutex = xSemaphoreCreateMutex();
  
  if (displayMutex == NULL || dataMutex == NULL) {
    LOG_ERROR_F("Failed to create mutexes!");
    return;
  }
  
  // Create display task
  #if USE_SINGLE_CORE
    // ESP32-C3: single core, use xTaskCreate
    xTaskCreate(
      displayTask,              // Task function
      "DisplayTask",           // Task name
      DISPLAY_TASK_STACK,      // Stack size
      NULL,                    // Parameters
      DISPLAY_TASK_PRIORITY,   // Priority
      &displayTaskHandle       // Task handle
    );
    LOG_INFO("Display task created (single core)");
  #else
    // ESP32: dual core, pin to Core 0
    xTaskCreatePinnedToCore(
      displayTask,              // Task function
      "DisplayTask",           // Task name
      DISPLAY_TASK_STACK,      // Stack size
      NULL,                    // Parameters
      DISPLAY_TASK_PRIORITY,   // Priority
      &displayTaskHandle,      // Task handle
      CORE_0                   // Core 0
    );
    LOG_INFO("Display task created on Core 0");
  #endif
  
  // Create data update task
  #if USE_SINGLE_CORE
    // ESP32-C3: single core, use xTaskCreate
    xTaskCreate(
      dataUpdateTask,          // Task function
      "DataUpdateTask",        // Task name
      DATA_UPDATE_TASK_STACK,  // Stack size
      NULL,                    // Parameters
      DATA_UPDATE_TASK_PRIORITY, // Priority
      &dataUpdateTaskHandle    // Task handle
    );
    LOG_INFO("Data update task created (single core)");
  #else
    // ESP32: dual core, pin to Core 1
    xTaskCreatePinnedToCore(
      dataUpdateTask,          // Task function
      "DataUpdateTask",        // Task name
      DATA_UPDATE_TASK_STACK,  // Stack size
      NULL,                    // Parameters
      DATA_UPDATE_TASK_PRIORITY, // Priority
      &dataUpdateTaskHandle,   // Task handle
      CORE_1                   // Core 1
    );
    LOG_INFO("Data update task created on Core 1");
  #endif
  
  // Create sensor task
  if (IS_DHT_CONNECTED) {
    #if USE_SINGLE_CORE
      xTaskCreate(
        sensorTask,            // Task function
        "SensorTask",          // Task name
        SENSOR_TASK_STACK,     // Stack size
        NULL,                  // Parameters
        SENSOR_TASK_PRIORITY,  // Priority
        &sensorTaskHandle      // Task handle
      );
      LOG_INFO("Sensor task created (single core)");
    #else
      xTaskCreatePinnedToCore(
        sensorTask,            // Task function
        "SensorTask",          // Task name
        SENSOR_TASK_STACK,     // Stack size
        NULL,                  // Parameters
        SENSOR_TASK_PRIORITY,  // Priority
        &sensorTaskHandle,      // Task handle
        CORE_1                 // Core 1
      );
      LOG_INFO("Sensor task created on Core 1");
    #endif
  }
  
  // Create MQTT task
  if (DeviceState::getInstance().isMqttEnabled()) {
    #if USE_SINGLE_CORE
      xTaskCreate(
        mqttTask,              // Task function
        "MQTTTask",            // Task name
        MQTT_TASK_STACK,       // Stack size
        NULL,                  // Parameters
        MQTT_TASK_PRIORITY,    // Priority
        &mqttTaskHandle        // Task handle
      );
      LOG_INFO("MQTT task created (single core)");
    #else
      xTaskCreatePinnedToCore(
        mqttTask,              // Task function
        "MQTTTask",            // Task name
        MQTT_TASK_STACK,       // Stack size
        NULL,                  // Parameters
        MQTT_TASK_PRIORITY,    // Priority
        &mqttTaskHandle,       // Task handle
        CORE_1                 // Core 1
      );
      LOG_INFO("MQTT task created on Core 1");
    #endif
  }
  
  LOG_INFO_F("FreeRTOS tasks initialized successfully");
  #if USE_SINGLE_CORE
    LOG_INFO("CPU: Single core (ESP32-C3) - tasks will share Core 0");
  #else
    LOG_INFO("CPU: Dual core (ESP32) - tasks distributed across cores");
  #endif
}

