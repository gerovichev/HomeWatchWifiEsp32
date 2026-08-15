#include "main_process.h"
#include "device_state.h"
#include "calendar_manager.h"
#include "data_lock.h"

#include "constants.h"
#include <WiFi.h>
#include <TimeLib.h> // For advanced time manipulation
#include <esp_task_wdt.h>

SemaphoreHandle_t dataMutex = NULL;

// WiFi reconnection tracking
static unsigned long lastWiFiCheck = 0;

// TLS handshakes plus JSON parsing run on this task's stack; keep headroom so a
// slightly larger API response cannot overflow it.
constexpr uint32_t DATA_TASK_STACK_BYTES = 12288;

// The task feeds the watchdog between each step of a cycle, so this bounds a
// single step rather than the whole cycle. The longest legitimate step is an
// actual OTA firmware download; everything else is a fetch that gives up after
// three attempts (~20s). Generous on purpose - a watchdog that fires on a slow
// network is worse than none.
constexpr uint32_t TASK_WDT_TIMEOUT_MS = 180000;

// The IDF already brings the TWDT up before app_main under the default Arduino
// config, so reconfigure is the normal path here. Calling init() first would
// also work but logs an "already initialized" error on every boot.
//
// idle_core_mask is cleared deliberately: the timeout has to cover a worst-case
// network cycle, and at that length idle-task starvation detection is
// meaningless anyway. What this watchdog is for is a wedged socket in
// dataUpdateTask.
static void setupTaskWatchdog() {
  esp_task_wdt_config_t wdtConfig = {};
  wdtConfig.timeout_ms = TASK_WDT_TIMEOUT_MS;
  wdtConfig.idle_core_mask = 0;
  wdtConfig.trigger_panic = true;

  esp_err_t err = esp_task_wdt_reconfigure(&wdtConfig);
  if (err == ESP_ERR_INVALID_STATE) {
    err = esp_task_wdt_init(&wdtConfig);
  }

  if (err == ESP_OK) {
    LOG_INFOF("Task watchdog armed at %lus", (unsigned long)(TASK_WDT_TIMEOUT_MS / 1000));
  } else {
    LOG_WARNINGF("Task watchdog setup failed: %s", esp_err_to_name(err));
  }
}

// Setup function, called once at startup
void setup() {
  Logger::getInstance().begin(115200);
  Logger::getInstance().setLogLevel(LOG_LEVEL_NONE); // Set desired log level

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

  setupTaskWatchdog();

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
    // Subscribe for the working part of the cycle only, and unsubscribe before
    // the long sleep below. Staying subscribed across a DATA_UPDATE_INTERVAL_SEC
    // (20 min) vTaskDelay guarantees the watchdog fires on a completely healthy
    // device, because a sleeping task never feeds it - a sleeping task is not a
    // hung task, so it should not be watched at all.
    const bool watched = (esp_task_wdt_add(NULL) == ESP_OK);
    if (!watched) {
      LOG_WARNING_F("Could not register dataUpdateTask with the task watchdog");
    }

    LOG_INFO_F("Starting data update cycle...");

    // Sensor I/O belongs here on the data core, not in the display path.
    Clock::getInstance().getDht22().refresh();

    if (WiFi.status() == WL_CONNECTED) {
      LOG_DEBUG_F("WiFi connected, updating services...");

      if (DeviceState::getInstance().isOtaRequired()) {
        LOG_DEBUG_F("Checking for OTA updates...");
        update_ota(); // Handle OTA updates
      }
      esp_task_wdt_reset();

      LOG_DEBUG_F("Updating location...");
      location_init();
      esp_task_wdt_reset();

      if (DeviceState::getInstance().isMqttEnabled()) {
        if (!client.connected()) {
          LOG_WARNING_F("MQTT disconnected, reconnecting...");
          reconnect(); // Reconnect to MQTT broker if needed
        }
        client.loop(); // Keep MQTT client running
        LOG_DEBUG_F("Publishing temperature to MQTT...");
        publish_temperature(); // Publish temperature to MQTT
      }
      esp_task_wdt_reset();

      LOG_DEBUG_F("Updating time from NTP...");
      timeClient.update(); // Update the time from NTP server
      {
        DataLock lock;
        timeNow = timeClient.getEpochTime();
        setTime(timeNow);
      }
      LOG_VERBOSE("Current epoch time: " + String(timeNow));

      LOG_DEBUG_F("Updating timezone...");
      getTimezone(); // Update timezone info
      esp_task_wdt_reset();

      LOG_DEBUG_F("Fetching weather data...");
      Clock::getInstance()
          .getWeatherManager()
          .readWeather(); // Fetch weather data
      esp_task_wdt_reset();

      LOG_DEBUG_F("Fetching currency rates...");
      Clock::getInstance()
          .getCurrencyManager()
          .initialize(); // Initialize currency data
      esp_task_wdt_reset();

      LOG_DEBUG_F("Updating calendar events...");
      CalendarManager &calendarManager = Clock::getInstance().getCalendarManager();
      if (calendarManager.shouldUpdateToday()) {
        calendarManager.readCalendarEvents(); // Update calendar events
      }

      // Display intensity is applied by loop() on the display core - see
      // setIntensityByTime() there.

      LOG_INFO_F("Data update cycle completed successfully");
    } else {
      LOG_ERROR_F("WiFi not connected, skipping data update");
    }

    // Stack headroom catches a creeping overflow; free/largest-block together
    // distinguish a genuine leak from heap fragmentation, which is the failure
    // mode for a device that builds Strings and runs for months.
    LOG_INFOF("Health: stack %uB free, heap %uB free, largest block %uB, uptime %lus",
              (unsigned)uxTaskGetStackHighWaterMark(NULL),
              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
              millis() / 1000);

    // Leave the watchdog's view before sleeping - see the note at the top of
    // the loop.
    if (watched) {
      esp_task_wdt_delete(NULL);
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
    time_t nowSnapshot;
    time_t sunriseSnapshot;
    time_t sunsetSnapshot;
    {
      DataLock lock;
      nowSnapshot = timeNow;
      sunriseSnapshot = sunrise;
      sunsetSnapshot = sunset;
    }
    setIntensityByTime(nowSnapshot, sunriseSnapshot, sunsetSnapshot);

    clock_loop();              // Handle clock logic
    realDisplayText();         // Update the display
  }

  delay(1); // Yield to WiFi/FreeRTOS
}

