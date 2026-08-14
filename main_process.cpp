#include "main_process.h"
#include "device_state.h"
#include "calendar_manager.h"

#include "constants.h"
#include <WiFi.h>
#include <TimeLib.h> // For advanced time manipulation

SemaphoreHandle_t dataMutex = NULL;

// WiFi reconnection tracking
static unsigned long lastWiFiCheck = 0;

// TLS handshakes plus JSON parsing run on this task's stack; keep headroom so a
// slightly larger API response cannot overflow it.
constexpr uint32_t DATA_TASK_STACK_BYTES = 12288;

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

  // Sync system clock via NTP before any TLS handshake - mbedTLS validates
  // certificate notBefore/notAfter dates against it, and rejects otherwise
  // valid certs if the clock is still at its power-on epoch.
  setClock();

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
      DATA_TASK_STACK_BYTES,
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
      if (dataMutex != NULL) xSemaphoreTake(dataMutex, portMAX_DELAY);
      timeNow = timeClient.getEpochTime();
      setTime(timeNow);
      if (dataMutex != NULL) xSemaphoreGive(dataMutex);
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

      // Display intensity is applied by loop() on the display core - see
      // setIntensityByTime() there.

      LOG_INFO_F("Data update cycle completed successfully");
      LOG_DEBUG("DataUpdate stack headroom: " +
                String(uxTaskGetStackHighWaterMark(NULL)) + " bytes");
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
  if (currentTime - lastWiFiCheck > Timing::WIFI_CHECK_INTERVAL_MS) {
    lastWiFiCheck = currentTime;

    if (WiFi.status() != WL_CONNECTED) {
      LOG_DEBUG_F("WiFi disconnected, attempting to reconnect...");
      WIFISetup wifiSetup;
      wifiSetup.attemptReconnect();
    }
  }

  if (displayAnimate()) {
    // timeNow/sunrise/sunset are 64-bit and written by dataUpdateTask on core 0,
    // so a bare read here can tear or catch sunrise mid-timezone-adjustment.
    // Snapshot them under the mutex, then work from the local copies.
    time_t nowSnapshot = timeNow;
    time_t sunriseSnapshot = sunrise;
    time_t sunsetSnapshot = sunset;
    if (dataMutex != NULL &&
        xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      nowSnapshot = timeNow;
      sunriseSnapshot = sunrise;
      sunsetSnapshot = sunset;
      xSemaphoreGive(dataMutex);
    }
    setIntensityByTime(nowSnapshot, sunriseSnapshot, sunsetSnapshot);

    clock_loop();              // Handle clock logic
    realDisplayText();         // Update the display
  }

  delay(1); // Yield to WiFi/FreeRTOS
}

