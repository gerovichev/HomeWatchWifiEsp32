#include "location_manager.h"
#include "constants.h"
#include "logger.h"
#include "secure_client.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include <WifiLocation.h>

// Define global variables
String ip;
float latitude = 31.66;
float longitude = 34.56;
Config config;
int maxAttemptsLoc = Retry::MAX_ATTEMPTS_LOCATION;

// Path for configuration file
const char *filenamecnf = "/config.txt";

// Loads the configuration from a file
void loadConfiguration() {
  // Try to mount SPIFFS
  if (!SPIFFS.begin(false)) {
    // If mount fails, try to format and mount again
    LOG_WARNING_F("SPIFFS mount failed, attempting to format...");
    if (!SPIFFS.format()) {
      LOG_ERROR_F("SPIFFS format failed, using defaults");
      return;
    }
    // Try to mount again after formatting
    if (!SPIFFS.begin(true)) {
      LOG_ERROR_F("SPIFFS mount failed after format, using defaults");
      return;
    }
    LOG_INFO_F("SPIFFS formatted and mounted successfully");
  }
  
  // Log SPIFFS info - optimized to avoid String concatenation
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("SPIFFS: "));
    Serial.print(usedBytes);
    Serial.print(F(" / "));
    Serial.print(totalBytes);
    Serial.println(F(" bytes used"));
  }

  File file = SPIFFS.open(filenamecnf, "r");
  if (file) {
    StaticJsonDocument<Buffer::JSON_LOCATION_SIZE>
        doc; // Location config is small: lat, lon, ip
    DeserializationError error = deserializeJson(doc, file);

    if (error) {
      LOG_WARNING_F("Failed to read location config file, using defaults");
    } else {
      config.latitude = doc["latitude"] | 0.0f;
      config.longitude = doc["longitude"] | 0.0f;
      const char* ipStr = doc["ip"] | "";
      config.ip = String(ipStr);
      // Optimized logging - avoid String concatenation
      if (Serial) {
        Serial.print(F("[INFO]   "));
        Serial.print(F(" "));
        Serial.print(F("Loaded location config: lat="));
        Serial.print(config.latitude, 6);
        Serial.print(F(", lon="));
        Serial.print(config.longitude, 6);
        Serial.print(F(", ip="));
        Serial.println(config.ip);
      }
    }

    file.close();
  } else {
    LOG_WARNING_F("Location config file not found");
  }

  SPIFFS.end();
}

// Saves the configuration to a file
void saveConfiguration() {
  // Try to mount SPIFFS
  if (!SPIFFS.begin(false)) {
    // If mount fails, try to format and mount again
    LOG_WARNING_F("SPIFFS mount failed, attempting to format...");
    if (!SPIFFS.format()) {
      LOG_ERROR_F("SPIFFS format failed, cannot save config");
      return;
    }
    // Try to mount again after formatting
    if (!SPIFFS.begin(true)) {
      LOG_ERROR_F("SPIFFS mount failed after format, cannot save config");
      return;
    }
    LOG_INFO_F("SPIFFS formatted and mounted successfully");
  }

  File file = SPIFFS.open(filenamecnf, "w");
  if (!file) {
    LOG_ERROR_F("Failed to create location config file");
    SPIFFS.end();
    return;
  }

  StaticJsonDocument<Buffer::JSON_LOCATION_SIZE>
      doc; // Location config is small: lat, lon, ip
  doc["latitude"] = config.latitude;
  doc["longitude"] = config.longitude;
  doc["ip"] = config.ip.c_str();

  if (serializeJson(doc, file) == 0) {
    LOG_ERROR_F("Failed to write location config to file");
  } else {
    // Optimized logging - avoid String concatenation
    if (Serial) {
      Serial.print(F("[INFO]   "));
      Serial.print(F(" "));
      Serial.print(F("Location config saved: lat="));
      Serial.print(config.latitude, 6);
      Serial.print(F(", lon="));
      Serial.print(config.longitude, 6);
      Serial.print(F(", ip="));
      Serial.println(config.ip);
    }
  }

  file.close();
  SPIFFS.end();
}

// Sets time via NTP for x.509 validation
void setClock() {
  LOG_DEBUG_F("Setting system clock for SSL validation...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int waitCount = 0;
  // Wait for time to be set, but with a timeout (approx 25 seconds)
  while (now < 8 * 3600 * 2 && waitCount < 50) {
    delay(Timing::NTP_SYNC_WAIT_MS);
    if (++waitCount % 10 == 0) {
      LOG_VERBOSE_F("Waiting for NTP time sync...");
    }
    now = time(nullptr);
  }

  if (waitCount >= 50) {
    LOG_WARNING_F("NTP sync timeout, continuing with unsynchronized time");
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[DEBUG]  "));
    Serial.print(F(" "));
    Serial.print(F("System clock set: "));
    Serial.println(asctime(&timeinfo));
  }
}

// Get location via Google API using WiFi data
void getLocationAPI(String ip) {
  LOG_INFO_F("Fetching location via Google Geolocation API...");

  setClock();

  WifiLocation location(googleApiKey);
  location_t loc = location.getGeoFromWiFi();

  if (!location.wlStatusStr(location.getStatus()).equals("OK")) {
    // Optimized logging - avoid String concatenation
    if (Serial) {
      Serial.print(F("[ERROR]  "));
      Serial.print(F(" "));
      Serial.print(F("Google Geolocation API returned status: "));
      Serial.println(location.wlStatusStr(location.getStatus()));
    }
    return;
  }

  latitude = loc.lat;
  longitude = loc.lon;

  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.print(F("Location updated: lat="));
    Serial.print(latitude, 7);
    Serial.print(F(", lon="));
    Serial.println(longitude, 7);
  }
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[DEBUG]  "));
    Serial.print(F(" "));
    Serial.print(F("Location accuracy: "));
    Serial.print(loc.accuracy);
    Serial.println(F(" meters"));
  }
  // Note: getSurroundingWiFiJson() returns String - disabled to save code size
  // LOG_VERBOSE("WiFi scan data: " + location.getSurroundingWiFiJson());

  config.latitude = latitude;
  config.longitude = longitude;
  config.ip = ip;
}

// Get external IP address using an API
String getIp() {
  LOG_INFO_F("Fetching external IP address...");

  String payload;
  WiFiClientSecure client;
  setupSecureClient(client, "ipify.org");
  HTTPClient http;

  String path = "https://api.ipify.org";
  int attempts = 0;
  bool success = false;

  while (attempts < maxAttemptsLoc && !success) {
    if (http.begin(client, path)) {
      // Optimized logging - avoid String concatenation
      if (Serial) {
        Serial.print(F("[DEBUG]  "));
        Serial.print(F(" "));
        Serial.print(F("IP retrieval attempt "));
        Serial.print(attempts + 1);
        Serial.print(F("/"));
        Serial.println(maxAttemptsLoc);
      }
      int httpCode = http.GET(); // Send the request

      if (httpCode == HTTP_CODE_OK) {
        payload = http.getString(); // Get the response payload
        // Optimized logging - avoid String concatenation
        if (Serial) {
          Serial.print(F("[INFO]   "));
          Serial.print(F(" "));
          Serial.print(F("External IP retrieved: "));
          Serial.println(payload);
        }
        success = true;
        maxAttemptsLoc = 1;
      } else {
        LOG_WARNING_VAR("IP retrieval HTTP error: ", httpCode);
      }

      http.end();
    } else {
      LOG_ERROR_F("Failed to begin IP retrieval HTTP connection");
    }

    if (!success) {
      attempts++;
      if (attempts < maxAttemptsLoc) {
        // Optimized logging - avoid String concatenation
        if (Serial) {
          Serial.print(F("[WARN]   "));
          Serial.print(F(" "));
          Serial.print(F("Retrying IP retrieval ("));
          Serial.print(attempts);
          Serial.print(F("/"));
          Serial.print(maxAttemptsLoc);
          Serial.println(F(")..."));
        }
        delay(Timing::RETRY_DELAY_MS);
      } else {
        // Optimized logging - avoid String concatenation
        if (Serial) {
          Serial.print(F("[ERROR]  "));
          Serial.print(F(" "));
          Serial.print(F("Failed to get IP after "));
          Serial.print(maxAttemptsLoc);
          Serial.println(F(" attempts."));
        }
        // Don't restart immediately - allow device to continue with cached
        // location if available Only restart if this is critical for device
        // operation
        if (config.latitude == 0 && config.longitude == 0) {
          LOG_ERROR_F("No cached location available, restarting device...");
          delay(1000);
          ESP.restart();
        } else {
          LOG_WARNING_F("Using cached location due to IP retrieval failure");
        }
      }
    }
  }

  return payload;
}

// Initialize location by loading config or calling API
void location_init() {
  LOG_INFO_F("Initializing location services...");

  ip = getIp();
  loadConfiguration();

  if (ip.equals(config.ip) && config.latitude != 0) {
    latitude = config.latitude;
    longitude = config.longitude;
    // Optimized logging - avoid String concatenation
    if (Serial) {
      Serial.print(F("[INFO]   "));
      Serial.print(F(" "));
      Serial.print(F("Using cached location: lat="));
      Serial.print(latitude, 7);
      Serial.print(F(", lon="));
      Serial.println(longitude, 7);
    }
  } else {
    LOG_INFO_F("IP changed or no cached location, fetching new location...");
    getLocationAPI(ip);
    saveConfiguration();
  }
}

