#include "WiFiSetup.h"

#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <esp_wifi.h>
#include <esp_mac.h>
#include "Secret.h"
#include "logger.h"
#include "led_display.h"
#include "constants.h"

// Check if WiFi credentials are saved in NVS
bool WIFISetup::hasSavedCredentials() {
    // Try to read saved SSID - this works even when not connected
    String savedSSID = WiFi.SSID();
    bool hasCredentials = (savedSSID.length() > 0);
    
    if (!hasCredentials) {
        wifi_config_t config;
        if (esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK) {
            hasCredentials = (strlen(reinterpret_cast<const char*>(config.sta.ssid)) > 0);
        }
    }
    
    LOG_DEBUG("Saved credentials check: " + String(hasCredentials ? "Found" : "Not found"));
    if (hasCredentials) {
        LOG_DEBUG("Saved SSID: " + savedSSID);
    }
    
    return hasCredentials;
}

// Attempt direct connection to saved WiFi credentials with retries
bool WIFISetup::attemptDirectConnection(int maxAttempts) {
    LOG_INFO_F("Attempting direct WiFi connection to saved credentials...");
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        LOG_INFO("WiFi connection attempt " + String(attempt) + "/" + String(maxAttempts));
        
        WiFi.mode(WIFI_STA);
        WiFi.begin();  // Uses saved credentials from NVS
        
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - startTime > NetworkConfig::WIFI_INIT_SINGLE_ATTEMPT_TIMEOUT_MS) {
                LOG_WARNING("Connection attempt " + String(attempt) + " timed out");
                break;
            }
            delay(500);
            yield();
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            LOG_INFO("✓ WiFi connected successfully on attempt " + String(attempt));
            return true;
        }
        
        if (attempt < maxAttempts) {
            LOG_WARNING("Connection failed, retrying in " + 
                       String(NetworkConfig::WIFI_INIT_RETRY_DELAY_MS / 1000) + " seconds...");
            delay(NetworkConfig::WIFI_INIT_RETRY_DELAY_MS);
        }
    }
    
    LOG_ERROR("Failed to connect after " + String(maxAttempts) + " attempts");
    return false;
}

namespace {
bool wifi_events_registered = false;

void registerWiFiEvents() {
    if (wifi_events_registered) {
        return;
    }

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_START:
                LOG_INFO_F("WiFi event: STA_START");
                break;
            case ARDUINO_EVENT_WIFI_STA_STOP:
                LOG_WARNING_F("WiFi event: STA_STOP");
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                LOG_INFO_F("WiFi event: STA_CONNECTED");
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                LOG_WARNING("WiFi event: STA_DISCONNECTED, reason=" + String(info.wifi_sta_disconnected.reason));
                break;
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                LOG_INFO("WiFi event: STA_GOT_IP " + WiFi.localIP().toString());
                break;
            default:
                LOG_DEBUG("WiFi event: " + String(static_cast<int>(event)));
                break;
        }
    });

    wifi_events_registered = true;
}
} // namespace

// Function to initialize and connect to WiFi
void WIFISetup::wifi_init() {
    LOG_INFO_F("Starting WiFi initialization...");
    registerWiFiEvents();
    // Ensure WiFi stack is initialized before any checks or WiFiManager.
    if (!WiFi.mode(WIFI_STA)) {
        LOG_ERROR_F("WiFi.mode(WIFI_STA) failed");
        return;
    }
    WiFi.setSleep(false);
    delay(50);
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
    LOG_DEBUG("Device MAC: " + macAddr);

    // First, try to connect using saved credentials with multiple retries
    if (hasSavedCredentials()) {
        LOG_INFO_F("Found saved WiFi credentials, attempting direct connection...");
        
        if (attemptDirectConnection(NetworkConfig::WIFI_INIT_RETRY_ATTEMPTS)) {
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            
            String connectedSSID = WiFi.SSID();
            printText(connectedSSID);
            delay(2000);
            
            LOG_INFO("✓ WiFi connected successfully!");
            LOG_INFO("  SSID: " + connectedSSID);
            LOG_INFO("  IP: " + WiFi.localIP().toString());
            LOG_INFO("  Gateway: " + WiFi.gatewayIP().toString());
            LOG_DEBUG("  Subnet: " + WiFi.subnetMask().toString());
            LOG_DEBUG("  DNS: " + WiFi.dnsIP().toString());
            LOG_DEBUG("  MAC: " + WiFi.macAddress());
            LOG_DEBUG("  RSSI: " + String(WiFi.RSSI()) + " dBm");
            LOG_DEBUG("  Channel: " + String(WiFi.channel()));
            return;
        } else {
            LOG_WARNING_F("WiFi connection failed, but saved credentials exist");
            LOG_INFO_F("Device will continue trying to reconnect in background");
            LOG_INFO_F("AP mode will NOT be started - device will keep retrying saved WiFi");
            printText(F("WiFi RETRY"));
            
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            return;
        }
    }
    
    // No saved credentials - use WiFiManager for initial setup
    LOG_INFO_F("No saved WiFi credentials found via esp_wifi/WiFi.SSID, starting configuration portal...");
    // Allow AP + STA for WiFiManager portal.
    WiFi.mode(WIFI_AP_STA);
    WiFiManager wifiManager;

    wifiManager.setConnectTimeout(180);
    wifiManager.setClass(F("invert"));
    LOG_DEBUG("Config portal SSID: " + String(wifi_name));

    LOG_INFO_F("Starting WiFi configuration portal (AP mode)...");
    LOG_INFO_F("WiFiManager may still try previously saved AP before portal");
    bool res = wifiManager.autoConnect(wifi_name, wifi_pass);

    if (!res) {
        LOG_ERROR_F("Failed to connect to WiFi or configuration timeout reached");
        LOG_WARNING_F("Device will continue with limited functionality");
        printText(F("WiFi FAIL"));
    } else {
        wifiManager.stopWebPortal();

        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);

        String connectedSSID = WiFi.SSID();
        printText(connectedSSID);
        delay(2000);

        LOG_INFO("✓ WiFi connected successfully!");
        LOG_INFO("  SSID: " + connectedSSID);
        LOG_INFO("  IP: " + WiFi.localIP().toString());
        LOG_INFO("  Gateway: " + WiFi.gatewayIP().toString());
        LOG_DEBUG("  Subnet: " + WiFi.subnetMask().toString());
        LOG_DEBUG("  DNS: " + WiFi.dnsIP().toString());
        LOG_DEBUG("  MAC: " + WiFi.macAddress());
        LOG_DEBUG("  RSSI: " + String(WiFi.RSSI()) + " dBm");
        LOG_DEBUG("  Channel: " + String(WiFi.channel()));
    }
}

// Function to reset saved WiFi credentials
void WIFISetup::wifi_reset() {
    LOG_WARNING_F("Resetting WiFi credentials...");
    LOG_WARNING("Current SSID: " + WiFi.SSID() + " will be forgotten");
    
    WiFiManager wifiManager;

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

// Attempt to reconnect to saved WiFi
bool WIFISetup::attemptReconnect() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    if (!hasSavedCredentials()) {
        LOG_DEBUG_F("No saved credentials for reconnection");
        return false;
    }
    
    LOG_INFO_F("Attempting to reconnect to saved WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > NetworkConfig::WIFI_CONNECT_TIMEOUT_MS) {
            LOG_DEBUG_F("Reconnection timeout");
            return false;
        }
        delay(500);
        yield();
    }
    
    LOG_INFO("WiFi reconnected! IP: " + WiFi.localIP().toString());
    return true;
}
