#include "location_manager.h"
#include "constants.h"
#include "logger.h"
#include "http_fetch.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WifiLocation.h>

// Define global variables
String ip;
float latitude = 31.66;
float longitude = 34.56;
Config config;


// Path for configuration file
const char *filenamecnf = "/config.txt";

// Mounts LittleFS without auto-formatting. Formatting is a destructive
// recovery step, so it only runs after a plain mount has actually failed and
// it says so in the log - otherwise a transient mount error silently wipes the
// cached location.
static bool mountLittleFS() {
  if (LittleFS.begin(false)) {
    return true;
  }

  LOG_WARNING_F("LittleFS mount failed, formatting (cached location will be lost)");
  if (LittleFS.begin(true)) {
    LOG_INFO_F("LittleFS formatted and mounted");
    return true;
  }

  LOG_ERROR_F("Failed to mount LittleFS even after formatting");
  return false;
}

// Loads the configuration from a file
void loadConfiguration() {
  if (!mountLittleFS()) {
    return;
  }

  File file = LittleFS.open(filenamecnf, "r");
  if (file) {
    JsonDocument doc; // Location config is small: lat, lon, ip
    DeserializationError error = deserializeJson(doc, file);

    if (error) {
      LOG_WARNING_F("Failed to read location config file, using defaults");
    } else {
      config.latitude = doc["latitude"] | 0.0f;
      config.longitude = doc["longitude"] | 0.0f;
      config.ip = String(doc["ip"] | "");
      LOG_INFO("Loaded location config: lat=" + String(config.latitude, 6) +
               ", lon=" + String(config.longitude, 6) + ", ip=" + config.ip);
    }

    file.close();
  } else {
    LOG_WARNING_F("Location config file not found");
  }

  LittleFS.end();
}

// Saves the configuration to a file
void saveConfiguration() {
  if (!mountLittleFS()) {
    return;
  }

  File file = LittleFS.open(filenamecnf, "w");
  if (!file) {
    LOG_ERROR_F("Failed to create location config file");
    return;
  }

  JsonDocument doc; // Location config is small: lat, lon, ip
  doc["latitude"] = config.latitude;
  doc["longitude"] = config.longitude;
  doc["ip"] = config.ip;

  if (serializeJson(doc, file) == 0) {
    LOG_ERROR_F("Failed to write location config to file");
  } else {
    LOG_INFO("Location config saved: lat=" + String(config.latitude, 6) +
             ", lon=" + String(config.longitude, 6) + ", ip=" + config.ip);
  }

  file.close();
  LittleFS.end();
}

// Sets time via NTP for x.509 validation
void setClock() {
  LOG_DEBUG_F("Setting system clock for SSL validation...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  constexpr int MAX_SYNC_POLLS = 50;  // x NTP_SYNC_WAIT_MS = approx 25 seconds

  time_t now = time(nullptr);
  int waitCount = 0;
  while (now < Timing::MIN_VALID_EPOCH && waitCount < MAX_SYNC_POLLS) {
    delay(Timing::NTP_SYNC_WAIT_MS);
    if (++waitCount % 10 == 0) {
      LOG_VERBOSE_F("Waiting for NTP time sync...");
    }
    now = time(nullptr);
  }

  if (now < Timing::MIN_VALID_EPOCH) {
    LOG_WARNING_F("NTP sync timeout, continuing with unsynchronized time (TLS will likely fail)");
    return;
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char stamp[32];
  // strftime rather than asctime(): asctime appends a newline that breaks the
  // one-record-per-line log format.
  strftime(stamp, sizeof(stamp), "%a %b %d %H:%M:%S %Y", &timeinfo);
  LOG_DEBUG("System clock set: " + String(stamp));
}

// Get location via Google API using WiFi data. Returns true only when config
// was actually updated, so the caller knows whether persisting it is safe.
bool getLocationAPI(const String &ip) {
  LOG_INFO_F("Fetching location via Google Geolocation API...");

  // Note: the clock is already synced by setClock() in setup(); WifiLocation
  // manages its own TLS client and does not need a second 25s sync here.

  WifiLocation location(googleApiKey);
  location_t loc = location.getGeoFromWiFi();

  if (!location.wlStatusStr(location.getStatus()).equals("OK")) {
    LOG_ERROR("Google Geolocation API returned status: " +
              location.wlStatusStr(location.getStatus()));
    return false;
  }

  latitude = loc.lat;
  longitude = loc.lon;

  LOG_INFO("Location updated: lat=" + String(latitude, 7) +
           ", lon=" + String(longitude, 7));
  LOG_DEBUG("Location accuracy: " + String(loc.accuracy) + " meters");
  LOG_VERBOSE("WiFi scan data: " + location.getSurroundingWiFiJson());

  config.latitude = latitude;
  config.longitude = longitude;
  config.ip = ip;
  return true;
}

// Get external IP address using an API
String getIp() {
  LOG_INFO_F("Fetching external IP address...");

  String result;

  HttpFetchOptions opts;
  opts.url = "https://api.ipify.org";
  opts.tag = "ipify.org";
  opts.timeoutMs = Timing::HTTP_TIMEOUT_MS;
  opts.maxAttempts = Retry::MAX_ATTEMPTS_LOCATION;

  // Note: a failed lookup never restarts the device - latitude/longitude carry
  // sane built-in defaults, so this degrades to slightly-off weather rather
  // than a reboot loop while the network is down. location_init() handles the
  // empty return.
  httpFetchWithRetry(opts, [&result](const String &payload) {
    String candidate = payload;
    candidate.trim();
    // A 200 with an empty body is still a failed lookup - treating it as
    // success would propagate an empty ip into the location cache.
    if (candidate.length() == 0) {
      LOG_WARNING_F("External IP response was empty");
      return false;
    }
    result = candidate;
    LOG_INFO("External IP retrieved: " + result);
    return true;
  });

  return result;
}

// Initialize location by loading config or calling API
void location_init() {
  LOG_INFO_F("Initializing location services...");

  ip = getIp();
  loadConfiguration();

  const bool haveCachedLocation = (config.latitude != 0 || config.longitude != 0);

  // An empty ip means ipify was unreachable, not that our location moved.
  // Treating it as a change would burn a Google Geolocation call and then
  // persist ip="" - which never matches again, so every later cycle would
  // repeat the same thing.
  if (ip.length() == 0) {
    if (haveCachedLocation) {
      latitude = config.latitude;
      longitude = config.longitude;
      LOG_WARNING("External IP unavailable, using cached location: lat=" +
                  String(latitude, 7) + ", lon=" + String(longitude, 7));
    } else {
      LOG_WARNING_F("External IP unavailable and no cached location, keeping current coordinates");
    }
    return;
  }

  if (ip.equals(config.ip) && haveCachedLocation) {
    latitude = config.latitude;
    longitude = config.longitude;
    LOG_INFO("Using cached location: lat=" + String(latitude, 7) +
             ", lon=" + String(longitude, 7));
    return;
  }

  LOG_INFO_F("IP changed or no cached location, fetching new location...");
  if (getLocationAPI(ip)) {
    saveConfiguration();
  } else {
    // Geolocation failed - do not overwrite a good cache with stale values.
    LOG_WARNING_F("Geolocation failed, cached location left untouched");
  }
}
