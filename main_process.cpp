#include "main_process.h"
#include "device_state.h"

#include "constants.h"
// #include "board_led.h"  // Disabled - board LED not used
#include "multicore_tasks.h"
#include <WiFi.h>
#include <TimeLib.h> // For advanced time manipulation
#include <Ticker.h>

bool isRunWeather = false;

Ticker updateDataTicker;

// Timer interrupt handler to trigger weather and currency updates
void IRAM_ATTR runAllUpdates() { 
  isRunWeather = true;
  dataUpdateRequested = true; // Signal data update task
}

// Setup function, called once at startup
void setup() {
  Logger::getInstance().begin(115200);
  Logger::getInstance().setLogLevel(LOG_LEVEL_INFO); // Set to INFO to reduce code size

  LOG_INFO_F("Starting HomeWatchWifi ESP32...");
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Version: "));
    Serial.println(version_prg);
  }

  LOG_INFO_F("Initializing device configuration...");
  initPerDevice();
  LOG_INFO_F("Device configuration loaded");
  
  // Board LED initialization disabled
  // LOG_INFO_F("Initializing board RGB LED...");
  // initBoardLED();
  
  LOG_INFO_F("Setting up LED matrix display...");
  matrixSetup();
  LOG_INFO_F("LED matrix display ready");

  // Optimize String concatenation
  String helloMsg = "Hello " + DeviceState::getInstance().getWatchName();
  displayTextInSetup(helloMsg);

  displayTextInSetup(version_prg);

  LOG_INFO_F("Initializing WiFi...");
  displayTextInSetup("Connect WIFI");

  WIFISetup wifiSetup;
  wifiSetup.wifi_init(); // Initialize Wi-Fi

  displayTextInSetup(WiFi.localIP().toString());

  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("WiFi connected! IP Address: "));
    Serial.println(WiFi.localIP());
  }
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("WiFi RSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
  }

  LOG_INFO_F("Initializing location services...");
  location_init();
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Location initialized: lat="));
    Serial.print(latitude, 6);
    Serial.print(F(", lon="));
    Serial.println(longitude, 6);
  }

  LOG_INFO_F("Initializing NTP time...");
  ntp_init();
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("NTP time synchronized: "));
    Serial.println(formatTime(timeNow));
  }

  LOG_INFO_F("Starting DHT22 sensor...");
  Dht22_manager &dht22_manager = Clock::getInstance().getDht22();
  dht22_manager.dht22Start(); // Start DHT22 sensor
  LOG_INFO_F("DHT22 sensor initialized");

  if (DeviceState::getInstance().isOtaRequired()) {
    LOG_INFO_F("Initializing OTA updates...");
    web_ota_init();
    LOG_INFO_F("OTA updates ready");
  } else {
    LOG_INFO_F("OTA updates disabled");
  }

  if (DeviceState::getInstance().isMqttEnabled()) {
    LOG_INFO_F("Initializing MQTT client...");
    setup_mqtt(); // Initialize MQTT client
    LOG_INFO_F("MQTT client initialized");
  } else {
    LOG_INFO_F("MQTT disabled");
  }

  timeNow = timeClient.getEpochTime(); // Get the current time

  // Initialize clock process (builds display sequence) - MUST be before initMulticoreTasks
  LOG_INFO_F("Initializing clock process...");
  init_clock_process();
  LOG_INFO_F("Clock process initialized");

  // Load initial data immediately (weather and currency)
  LOG_INFO_F("Loading initial weather and currency data...");
  Clock::getInstance().getWeatherManager().readWeather();
  LOG_INFO_F("Weather data loaded");
  Clock::getInstance().getCurrencyManager().initialize();
  LOG_INFO_F("Currency data loaded");

  isRunWeather = true; // Set flag to trigger weather updates

  // printCityToScreen();  // Display the city

  // Initialize multicore tasks for parallel processing
  initMulticoreTasks();
  
  // Set up ticker to trigger data updates (tasks will handle it)
  updateDataTicker.attach(Timing::DATA_UPDATE_INTERVAL_SEC, runAllUpdates);
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Data update interval set to "));
    Serial.print(Timing::DATA_UPDATE_INTERVAL_SEC);
    Serial.println(F(" seconds"));
  }

  LOG_INFO_F("=== Setup completed successfully! ===");
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Free heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
  }
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Uptime: "));
    Serial.print(millis() / 1000);
    Serial.println(F(" seconds"));
  }
  #if USE_SINGLE_CORE
    LOG_INFO_F("CPU: Single core (ESP32-C3) - tasks share Core 0");
  #else
    LOG_INFO_F("CPU: Dual core (ESP32) - Core 0: Display, Core 1: Data/Sensors/MQTT");
  #endif
}

// Function to fetch weather and currency data
// NOTE: With multicore tasks, this function just signals the data update task
// The actual work is done in dataUpdateTask on Core 1
void fetchWeatherAndCurrency() {
  if (isRunWeather) {
    isRunWeather = false;
    dataUpdateRequested = true; // Signal data update task on Core 1
    LOG_DEBUG_F("Data update requested (will be handled by Core 1 task)");
  }
}

// Main loop, called repeatedly
// Note: With multicore tasks, main loop is simplified
// Display updates are handled by displayTask on Core 0
// Data updates are handled by dataUpdateTask on Core 1
void loop() {
  // Main loop now just yields to allow FreeRTOS scheduler
  // All work is done in separate tasks on different cores
  vTaskDelay(pdMS_TO_TICKS(100)); // Yield to other tasks
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
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("WiFi reconnected! IP: "));
    Serial.println(WiFi.localIP());
  }
}

// Function to disable Wi-Fi
void disableWiFi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  LOG_INFO_F("WiFi disabled to save power");
}

