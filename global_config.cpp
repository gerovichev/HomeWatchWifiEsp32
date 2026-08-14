#include "global_config.h"
#include "constants.h"
#include <WiFi.h>
#include <esp_mac.h>

// Define the global variables
String lang_weather;
time_t sunrise;
time_t sunset;

String version_prg = "260710";
char grad = '\x60';

float humidity_delta = 0.00;
String hostname_m;

boolean isOTAreq = true;
boolean isMQTT = false;
String nameofWatch;

String macAddrSt;
String daysOfTheWeek[7];
boolean IS_DHT_CONNECTED = false;
boolean isWebClientNeeded = true;
boolean isReadWeather = true;

// Function to return the degree character based on the language
char getGradValue() {
  return grad;
}

// Initialize the device configuration based on the MAC address
void initPerDevice() {
  setDeviceConfig();  // Load configuration for the device

  uint8_t mac[6];
  String macAddr;
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    macAddr = String(macStr);
  } else {
    macAddr = WiFi.macAddress();
  }
  LOG_INFO("MAC: " + macAddr);

  macAddrSt = macAddr;

  // If the device MAC is found in the configuration map, apply the settings
  auto entry = configMap.find(macAddr);
  if (entry != configMap.end()) {
    const DeviceConfig& config = entry->second;

    lang_weather = config.lang_weather;
    hostname_m = config.hostname_m;
    IS_DHT_CONNECTED = config.IS_DHT_CONNECTED;
    isWebClientNeeded = config.isWebClientNeeded;
    isReadWeather = config.isReadWeather;
    humidity_delta = config.humidity_delta;
    nameofWatch = config.nameofWatch;
    isOTAreq = config.isOTAreq;
    isMQTT = config.isMQTT;
    setIntensity(config.intensity);  // Set LED intensity based on the config

    LOG_INFO("Device configured: " + hostname_m);
    LOG_DEBUG("Language: " + lang_weather);
    LOG_DEBUG("DHT22: " + String(IS_DHT_CONNECTED ? "connected" : "disconnected"));
    LOG_DEBUG("MQTT: " + String(isMQTT ? "enabled" : "disabled"));

  } else {
    // Unknown board: set *every* field explicitly rather than leaving some at
    // their file-scope initialisers. In particular isOTAreq must default off -
    // an unidentified device should not pull firmware from the update server.
    lang_weather = "en";
    hostname_m = "ESP_Unknown";
    IS_DHT_CONNECTED = false;
    isWebClientNeeded = true;
    isReadWeather = true;
    humidity_delta = Sensor::HUMIDITY_DELTA_DEFAULT;
    nameofWatch = "New";
    isOTAreq = false;
    isMQTT = false;
    setIntensity(Display::INTENSITY_NIGHT);

    LOG_WARNING("MAC address not found in config, using defaults (OTA and MQTT disabled)");
  }

  // Derived from hostname_m, so it has to be built on both paths.
  mqtt_topic_str = hostname_m + String(mqtt_topic);

  LOG_INFO("Hostname: " + hostname_m);

  // Set days of the week based on language. The tables are static const so the
  // literals stay in flash instead of building 7 temporary Strings per branch.
  static const char* const daysRu[7] = { "Вс.", "Пн.", "Вт.", "Ср.", "Чт.", "Пт.", "Сб." };
  static const char* const daysEn[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  const char* const* days = (lang_weather == "ru") ? daysRu : daysEn;
  for (int i = 0; i < 7; i++) daysOfTheWeek[i] = days[i];
}

// Function to get a two-digit number as a string (with leading zero if necessary)
String getNumberWithZerro(int dig) {
    return (dig < 10) ? "0" + String(dig, DEC) : String(dig, DEC);
}

// Wrapper function for drawing text on the display
void drawString(const String& tape) {
  drawStringMax(tape);
}
