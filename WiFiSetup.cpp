#include "WiFiSetup.h"

#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <esp_mac.h>
#include "Secret.h"
#include "logger.h"
#include "led_display.h"
// #include "board_led.h"  // Disabled - board LED not used

// Helper function to get MAC address for ESP32
String getMacAddressForLog() {
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err == ESP_OK) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
  } else {
    return WiFi.macAddress();
  }
}

// Function to initialize and connect to WiFi using WiFiManager
void WIFISetup::wifi_init() {
    LOG_INFO_F("Starting WiFi initialization...");
    LOG_DEBUG("Device MAC: " + getMacAddressForLog());

    WiFiManager wifiManager;

    // Optional: Set connect timeout to 180 seconds
    wifiManager.setConnectTimeout(180);

    // Set dark theme for WiFiManager web interface
    wifiManager.setClass(F("invert"));

    LOG_DEBUG("Config portal SSID: " + String(wifi_name));

    // Try to auto-connect using saved credentials or start the AP mode
    LOG_INFO_F("Attempting WiFi connection (saved credentials or AP mode)...");
    bool res = wifiManager.autoConnect(wifi_name, wifi_pass);

    if (!res) {
        LOG_ERROR_F("Failed to connect to WiFi or timeout reached");
        LOG_WARNING_F("Device will restart or continue with limited functionality");
        // changeLEDColorForDisplay(DISPLAY_ERROR);  // Disabled - board LED not used
        printText(F("WiFi FAIL"));
        // Optionally restart the ESP device if connection fails
        // ESP.restart();
    } else {
        // Stop the configuration web portal
        wifiManager.stopWebPortal();

        // Set WiFi auto-reconnect and persistence
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);

        // Display connected network name on LED display
        String connectedSSID = WiFi.SSID();
        printText(connectedSSID);
        delay(2000);  // Show SSID for 2 seconds

        // Log connection information - simplified to reduce code size
        LOG_INFO("WiFi connected: " + connectedSSID);
        LOG_INFO("IP: " + WiFi.localIP().toString());
    }
}

// Function to reset saved WiFi credentials
void WIFISetup::wifi_reset() {
    LOG_WARNING_F("Resetting WiFi credentials...");
    LOG_WARNING("Current SSID: " + WiFi.SSID() + " will be forgotten");
    
    WiFiManager wifiManager;

    // Reset saved WiFi credentials
    wifiManager.resetSettings();
    LOG_INFO_F("WiFi credentials reset successfully");
    LOG_INFO_F("Device will need to be reconfigured on next boot");
}

// Check if WiFi is connected
bool WIFISetup::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Get WiFi status as a readable string
String WIFISetup::getStatusString() {
    switch (WiFi.status()) {
        case WL_CONNECTED:
            return "Connected";
        case WL_NO_SSID_AVAIL:
            return "SSID not available";
        case WL_CONNECT_FAILED:
            return "Connection failed";
        case WL_IDLE_STATUS:
            return "Idle";
        case WL_DISCONNECTED:
            return "Disconnected";
        default:
            return "Unknown (" + String(WiFi.status()) + ")";
    }
}

