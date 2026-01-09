#include "global_config.h"
#include <WiFi.h>
#include <esp_mac.h>

// Define the global variables
String lang_weather;
unsigned int sunrise;
unsigned int sunset;

String version_prg = "251219";
char grad = '\x60';

float humidity_delta = 0.00;
String hostname_m;

boolean isOTAreq = true;
boolean isMQTT = false;
String nameofWatch;

String macAddrSt;
String daysOfTheWeek[7];
boolean IS_DHT_CONNECTED = false;
bool isWebClientNeeded = true;
boolean isReadWeather = true;
int displayIntensity = 2;  // Default intensity value

// Function to return the degree character based on the language
char getGradValue() {
  return grad;
}

// Helper function to get MAC address for ESP32
String getMacAddress() {
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err == ESP_OK) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
  } else {
    // Fallback: try WiFi.macAddress() if WiFi is initialized
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
    }
    return WiFi.macAddress();
  }
}

// Initialize the device configuration based on the MAC address
void initPerDevice() {
  setDeviceConfig();  // Load configuration for the device

  String macAddr = getMacAddress();
  LOG_INFO("MAC: " + macAddr);

  macAddrSt = macAddr;

  // If the device MAC is found in the configuration map, apply the settings
  if (configMap.find(macAddr) != configMap.end()) {
    DeviceConfig& config = configMap[macAddr];

    lang_weather = config.lang_weather;
    hostname_m = config.hostname_m;
    IS_DHT_CONNECTED = config.IS_DHT_CONNECTED;
    isWebClientNeeded = config.isWebClientNeeded;
    isReadWeather = config.isReadWeather;
    humidity_delta = config.humidity_delta;
    nameofWatch = config.nameofWatch;
    isOTAreq = config.isOTAreq;
    isMQTT = config.isMQTT;
    displayIntensity = config.intensity;  // Store intensity from config
    setIntensity(displayIntensity);  // Set LED intensity based on the config
    mqtt_topic_str = hostname_m + String(mqtt_topic);
    
    LOG_INFO("Device configured: " + hostname_m);
    LOG_DEBUG("Language: " + lang_weather);
    LOG_DEBUG("DHT22: " + String(IS_DHT_CONNECTED ? "connected" : "disconnected"));
    LOG_DEBUG("MQTT: " + String(isMQTT ? "enabled" : "disabled"));

  } else {
    // Set default values if MAC address is not found in the config map
    lang_weather = "en";
    hostname_m = "ESP_Unknown";
    IS_DHT_CONNECTED = false;
    isWebClientNeeded = true;
    isReadWeather = true;
    nameofWatch = "New";
    
    LOG_WARNING("MAC address not found in config, using defaults");
  }

  LOG_INFO("Hostname: " + hostname_m);

  // Set days of the week based on language
  if (!lang_weather.compareTo("ru")) {
    String daysOfTheWeekT[7] = { "Вс.", "Пн.", "Вт.", "Ср.", "Чт.", "Пт.", "Сб." };
    for (int i = 0; i < 7; i++) daysOfTheWeek[i] = daysOfTheWeekT[i];
  } else {
    String daysOfTheWeekT[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    for (int i = 0; i < 7; i++) daysOfTheWeek[i] = daysOfTheWeekT[i];
  }
}

// Function to verify Wi-Fi connection
void verifyWifi() {
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    WiFi.reconnect();
  }
}

// Function to get a two-digit number as a string (with leading zero if necessary)
String getNumberWithZerro(int dig) {
    return (dig < 10) ? "0" + String(dig, DEC) : String(dig, DEC);
}

// Wrapper function for drawing text on the display
void drawString(String tape) {
  drawStringMax(tape);
}

