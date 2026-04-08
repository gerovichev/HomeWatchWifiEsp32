#include "main_process.h"
#include "device_state.h"
#include "calendar_manager.h"

#include "constants.h"
#include <WiFi.h>
#include <TimeLib.h> // For advanced time manipulation

SemaphoreHandle_t dataMutex = NULL;

// WiFi reconnection tracking
static unsigned long lastWiFiCheck = 0;
constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 60000;  // Check WiFi every 60 seconds

// Setup function, called once at startup
void setup() {
  Logger::getInstance().begin(115200);
  Logger::getInstance().setLogLevel(LOG_LEVEL_DEBUG); // Set desired log level

  LOG_INFO_F("Starting HomeWatchWifi ESP32...");
  LOG_INFO("Version: " + version_prg);

  initPerDevice();
  matrixSetup();

  // Optimize String concatenation
  String helloMsg;
  helloMsg.reserve(10 + DeviceState::getInstance().getWatchName().length());
  helloMsg = F("Hello ");
  helloMsg += DeviceState::getInstance().getWatchName();
  displayTextInSetup(helloMsg);

  displayTextInSetup(version_prg);

  LOG_INFO_F("Initializing WiFi...");
  displayTextInSetup(F("Connect WIFI"));

  WIFISetup wifiSetup;
  wifiSetup.wifi_init(); // Initialize Wi-Fi

  displayTextInSetup(WiFi.localIP().toString());

  LOG_INFO("IP Address: " + WiFi.localIP().toString());

  LOG_INFO_F("Initializing location services...");
  location_init();

  LOG_INFO_F("Initializing NTP time...");
  ntp_init();

  LOG_INFO_F("Starting DHT22 sensor...");
  Dht22_manager &dht22_manager = Clock::getInstance().getDht22();
  dht22_manager.dht22Start(); // Start DHT22 sensor

  if (DeviceState::getInstance().isOtaRequired()) {
    LOG_INFO_F("Initializing OTA updates...");
    web_ota_init();
  }

  if (DeviceState::getInstance().isMqttEnabled()) {
    LOG_INFO_F("Initializing MQTT client...");
    setup_mqtt(); // Initialize MQTT client
  }

  timeNow = timeClient.getEpochTime(); // Get the current time

  // Initialize calendar events
  LOG_INFO_F("Initializing calendar events...");
  Clock::getInstance().getCalendarManager().readCalendarEvents();

  // printCityToScreen();  // Display the city

  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex != NULL) {
    // Start data update task on Core 0
    xTaskCreatePinnedToCore(
      dataUpdateTask,
      "DataUpdate",
      8192,
      NULL,
      1,
      NULL,
      0 // Core 0
    );
  } else {
    LOG_ERROR_F("Failed to create dataMutex!");
  }

  init_clock_process();

  LOG_INFO_F("Setup completed successfully!");
}

// Background task to fetch weather and currency data
void dataUpdateTask(void *pvParameters) {
  while (true) {
    LOG_INFO_F("Starting data update cycle...");

    if (WiFi.status() == WL_CONNECTED) {
      LOG_DEBUG_F("WiFi connected, updating services...");

      if (DeviceState::getInstance().isOtaRequired()) {
        LOG_DEBUG_F("Checking for OTA updates...");
        update_ota(); // Handle OTA updates
      }

      LOG_DEBUG_F("Updating location...");
      location_init();

      if (DeviceState::getInstance().isMqttEnabled()) {
        if (!client.connected()) {
          LOG_WARNING_F("MQTT disconnected, reconnecting...");
          reconnect(); // Reconnect to MQTT broker if needed
        }
        client.loop(); // Keep MQTT client running
        LOG_DEBUG_F("Publishing temperature to MQTT...");
        publish_temperature(); // Publish temperature to MQTT
      }

      LOG_DEBUG_F("Updating time from NTP...");
      timeClient.update(); // Update the time from NTP server
      timeNow = timeClient.getEpochTime();
      setTime(timeNow);
      LOG_VERBOSE("Current epoch time: " + String(timeNow));

      LOG_DEBUG_F("Updating timezone...");
      getTimezone(); // Update timezone info

      LOG_DEBUG_F("Fetching weather data...");
      Clock::getInstance()
          .getWeatherManager()
          .readWeather(); // Fetch weather data

      LOG_DEBUG_F("Fetching currency rates...");
      Clock::getInstance()
          .getCurrencyManager()
          .initialize(); // Initialize currency data

      LOG_DEBUG_F("Updating calendar events...");
      CalendarManager &calendarManager = Clock::getInstance().getCalendarManager();
      if (calendarManager.shouldUpdateToday()) {
        calendarManager.readCalendarEvents(); // Update calendar events
      }

      LOG_DEBUG_F("Adjusting display intensity...");
      setIntensityByTime(timeNow); // Adjust display intensity based on time

      LOG_INFO_F("Data update cycle completed successfully");
    } else {
      LOG_ERROR_F("WiFi not connected, skipping data update");
    }

    // Wait for the next update interval
    // We use vTaskDelay to block the task efficiently for the given interval
    vTaskDelay(pdMS_TO_TICKS(Timing::DATA_UPDATE_INTERVAL_SEC * 1000));
  }
}

// Main loop, called repeatedly
void loop() {
  // Проверяем изменение минуты на каждой итерации (независимо от
  // displayAnimate)
  Clock::getInstance().checkMinuteChange();

  // Periodically check WiFi connection and attempt reconnection if needed
  unsigned long currentTime = millis();
  if (currentTime - lastWiFiCheck > WIFI_CHECK_INTERVAL_MS) {
    lastWiFiCheck = currentTime;
    
    if (WiFi.status() != WL_CONNECTED) {
      LOG_DEBUG_F("WiFi disconnected, attempting to reconnect...");
      WIFISetup wifiSetup;
      wifiSetup.attemptReconnect();
    }
  }

  if (displayAnimate()) {
    clock_loop();              // Handle clock logic
    realDisplayText();         // Update the display
  }

  delay(1); // Yield to WiFi/FreeRTOS
}

// Function to enable Wi-Fi (if disabled)
void enableWiFi() {
  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    LOG_DEBUG_F("WiFi already connected");
    return;
  }

  WiFi.begin(); // Reconnect using saved credentials
  LOG_INFO_F("Reconnecting to WiFi...");

  unsigned long startAttemptTime = millis();
  int dotCount = 0;

  while (WiFi.status() != WL_CONNECTED) {
    // Check for timeout
    if (millis() - startAttemptTime > Timing::WIFI_TIMEOUT_MS) {
      LOG_ERROR_F("WiFi connection timeout");
      return;
    }

    // Visual feedback every 500ms
    if (++dotCount % 5 == 0) {
      LOG_VERBOSE_F("Still connecting...");
    }
    yield(); // Allow other tasks to run instead of blocking delay
  }

  // Connection successful
  LOG_INFO("WiFi reconnected! IP: " + WiFi.localIP().toString());
}

// Function to disable Wi-Fi
void disableWiFi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  LOG_INFO_F("WiFi disabled to save power");
}
