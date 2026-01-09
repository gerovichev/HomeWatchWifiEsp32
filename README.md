# HomeWatchWifi ESP32

A smart watch project based on ESP32 with Max72xxPanel LED display that shows time, weather, currency rates, and environmental sensor data.

![License](https://img.shields.io/badge/license-GPLv3-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)

## Features

- **Time and Date Display**: Real-time clock with NTP synchronization
- **Weather Information**: Current weather, temperature, humidity, pressure from OpenWeatherMap API
- **Currency Rates**: USD and EUR exchange rates
- **Environmental Sensors**: Support for DHT11/DHT22 temperature and humidity sensor
- **Geolocation Services**: Automatic location detection via Google Geolocation API
- **Multi-language Support**: Interface in Russian and English
- **Energy Efficiency**: WiFi power management and automatic brightness adjustment
- **OTA Updates**: Over-the-air firmware updates
- **MQTT Integration**: Temperature data publishing to MQTT broker
- **Multi-device Support**: Configuration for multiple devices by MAC addresses
- **Centralized Logging**: Unified logging system with configurable levels
- **Error Handling**: Robust error handling with graceful degradation
- **Multicore/Multithreading Support**: FreeRTOS tasks for parallel processing (dual-core ESP32 and single-core ESP32-C3 compatible)

## Hardware Requirements

- ESP32 or ESP32-C3 microcontroller
- Max72xxPanel LED matrix display (4 modules, configurable via `MAX_DEVICES` in `led_display.h`)
- DHT11/DHT22 temperature/humidity sensor (optional, configurable in `dht22_manager.h`)
- WiFi connection

**Note**: The project automatically detects single-core (ESP32-C3) and dual-core (ESP32) configurations and adjusts task scheduling accordingly.

## Pin Configuration

### ESP32 (Standard)
- **CLK_PIN**: 18 (GPIO18)
- **DATA_PIN**: 23 (GPIO23)
- **CS_PIN**: 5 (GPIO5)
- **DHTPIN**: 12 (GPIO12) - DHT22 sensor

### ESP32-C3 (GOOUUU Board)
- **CLK_PIN**: 6 (GPIO6) - SPI CLK
- **DATA_PIN**: 7 (GPIO7) - SPI MOSI
- **CS_PIN**: 5 (GPIO5) - SPI CS
- **DHTPIN**: 2 (GPIO2) - DHT sensor

**Note**: 
- Pins can be changed in `led_display.h` and `dht22_manager.h` according to your ESP32 board
- The project includes specific configurations for ESP32-C3 boards (GOOUUU board)
- LED matrix hardware type is set to `FC16_HW` by default (configurable in `led_display.h`)
- Number of LED modules can be changed via `MAX_DEVICES` in `led_display.h` (default: 4)

## Prerequisites

- Arduino IDE or PlatformIO
- ESP32 board support package
- Required libraries (see below)

## Required Libraries

- `WiFi` (included in ESP32 package)
- `HTTPClient` (included in ESP32 package)
- `WiFiClientSecure` (included in ESP32 package)
- `ArduinoJson` (for JSON parsing)
- `MD_MAX72XX` (for LED matrix display)
- `MD_Parola` (for text scrolling on display)
- `TimeLib` (for time management)
- `DHT sensor library` (for DHT11/DHT22 sensor)
- `Ticker` (for periodic tasks)
- `WiFiManager` (for WiFi setup)
- `NTPClient` (for time synchronization)
- `PubSubClient` (for MQTT)
- `WifiLocation` (for location detection)
- `SPIFFS` (for file system)
- `FreeRTOS` (included in ESP32 package, for multicore task management)

## Installation Instructions

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/HomeWatchWifiEsp32.git
   cd HomeWatchWifiEsp32
   ```

2. **Configure API keys**: Create `Secret.cpp` based on the `Secret.h` template with your API credentials. Template example:

   ```cpp
   #include "Secret.h"
   
   // API Keys and URLs - Replace with your actual credentials
   const char* googleApiKey = "YOUR_GOOGLE_API_KEY_HERE";
   const char* confPathCurrencyUSD = "https://your-api.com/currency/usd";
   const char* confPathCurrencyEUR = "https://your-api.com/currency/eur";
   const char* confBearerTokenCurrency = "YOUR_BEARER_TOKEN_HERE";
   const char* appidWeather = "YOUR_OPENWEATHER_API_KEY_HERE";
   const char* apiKeyTimezone = "YOUR_TIMEZONE_API_KEY_HERE";
   const char* webOTA_updateURL = "https://your-server.com/ota/update";
   
   // MQTT broker credentials (optional, if using MQTT)
   const char* mqtt_server = "192.168.1.100";
   const char* mqtt_user = "mqtt_username";
   const char* mqtt_password = "mqtt_password";
   const char* mqtt_topic = "/temperature";
   String mqtt_topic_str;
   
   // WiFi credentials for AutoConnectAP (fallback)
   const char* wifi_name = "AutoConnectAP";
   const char* wifi_pass = "your_wifi_password";
   
   // Global device configuration map
   std::map<String, DeviceConfig> configMap;
   
   // Function to set device configurations based on MAC address
   void setDeviceConfig() {
       // Example device configuration
       // Replace MAC address with your device's MAC address
       DeviceConfig config1;
       config1.lang_weather = "en";              // "en" or "ru"
       config1.hostname_m = "ESP_Device1";
       config1.IS_DHT_CONNECTED = true;         // true if DHT22 sensor is connected
       config1.isWebClientNeeded = false;       // true if web client is needed
       config1.isReadWeather = true;            // true to fetch weather data
       config1.humidity_delta = 0.0;             // Humidity correction value
       config1.intensity = 1;                    // Display intensity (0-15)
       config1.isOTAreq = true;                  // true to enable OTA updates
       config1.nameofWatch = "Device Name";     // Name displayed on device
       config1.isMQTT = false;                   // true to enable MQTT
       
       // Add configuration for your device's MAC address
       // Find MAC address in Serial monitor on first boot
       configMap["AA:BB:CC:DD:EE:FF"] = config1;
       
       // Add more device configurations as needed
       // DeviceConfig config2;
       // ... settings ...
       // configMap["11:22:33:44:55:66"] = config2;
   }
   ```

   **Important**: 
   - `Secret.cpp` is not tracked by git (it's in `.gitignore`)
   - `Secret.h` is a template file and can be published
   - Never commit your real API keys to the repository
   - To find your device's MAC address, check the Serial monitor on first boot

3. **Device Configuration**: 
   - Modify device configurations in `Secret.cpp` for your specific devices
   - Add your device's MAC address and settings to the `setDeviceConfig()` function
   - You can find your device's MAC address in the Serial monitor on first boot
   - See example above for all available configuration options

4. **Upload to ESP32**: 
   - Select your ESP32 board in Arduino IDE
   - Select the correct COM port
   - Compile and upload the code

5. **WiFi Setup**: 
   - Connect to AutoConnectAP network
   - Configure WiFi credentials via web interface

## API Services Used

- **OpenWeatherMap**: Weather data
- **Google Geolocation API**: Geolocation services
- **TimezoneDB**: Timezone information
- **Custom Currency API**: Currency exchange rates
- **NTP Servers**: Time synchronization

## Display Cycle

The device cyclically displays the following information every 5 seconds:
1. Current time
2. Date
3. Day of week
4. Current temperature
5. "Feels like" temperature
6. Atmospheric pressure
7. Humidity
8. Weather description
9. USD exchange rate
10. EUR exchange rate
11. Home temperature (DHT sensor)
12. Home humidity (DHT sensor)

**Note**: Weather and currency data are updated every 15 minutes (configurable in `constants.h` as `Timing::DATA_UPDATE_INTERVAL_SEC`).

## Configuration

The project supports multiple device configurations based on MAC addresses. Each device can have different settings for:
- Language (Russian/English)
- Device name
- Sensor connections
- MQTT settings
- OTA update preferences
- Display intensity

## Power Management

- WiFi automatically disconnects after data updates to save energy
- Display brightness adjusts based on sunrise/sunset times
- Efficient operation based on timers

## Project Structure

```
HomeWatchWifiEsp32/
├── HomeWatchWifiEsp32.ino      # Main entry point
├── main_process.cpp/h          # Main process logic
├── multicore_tasks.cpp/h       # FreeRTOS multicore task management
├── clock.cpp/h                 # Clock and display management
├── weather_manager.cpp/h       # Weather data processing
├── currency_manager.cpp/h      # Currency rate processing
├── location_manager.cpp/h      # Geolocation services
├── TimeManager.cpp/h           # Time synchronization
├── dht22_manager.cpp/h         # DHT22 sensor management
├── led_display.cpp/h           # LED display functions
├── logger.cpp/h                # Logging system
├── error_handler.cpp/h         # Error handling
├── device_state.cpp/h          # Device state management
├── constants.h                 # Centralized constants
├── secure_client.h             # SSL client configuration
├── global_config.cpp/h         # Global configuration
├── WiFiSetup.cpp/h             # WiFi setup
├── OTAUpdate.cpp/h             # OTA updates
├── MQTTClient.cpp/h            # MQTT client
├── Secret.h                    # API keys template
├── Secret.cpp                   # API keys (not in git)
└── fonts.h                     # Font definitions
```

## Architecture

### Multicore/Multithreading Support

The project uses FreeRTOS tasks for efficient parallel processing:

- **Dual-core ESP32**: 
  - Core 0: Display updates (high priority)
  - Core 1: Data fetching, sensor reading, MQTT (medium priority)
  
- **Single-core ESP32-C3**: 
  - All tasks share Core 0 with priority-based scheduling
  - Automatic detection and task configuration

Tasks include:
- Display task: Handles LED matrix updates and animations
- Data update task: Fetches weather and currency data via HTTP
- Sensor task: Reads DHT22 sensor data
- MQTT task: Publishes sensor data to MQTT broker

Thread-safe operations use mutexes for shared resource access.

## Differences from ESP8266 Version

Main changes for ESP32:
- Using `WiFi.h` instead of `ESP8266WiFi.h`
- Using `HTTPClient.h` instead of `ESP8266HTTPClient.h`
- Using `WiFiClientSecure` instead of `WiFiClientSecureBearSSL`
- Using `httpUpdate.h` instead of `ESP8266httpUpdate.h`
- Using `SPIFFS` instead of `LittleFS`
- Different GPIO pins (configured in `led_display.h`)
- Using `vTaskDelay()` instead of `ESP.wdtFeed()` for yield
- Multicore support with FreeRTOS tasks for parallel processing

## Troubleshooting

### WiFi Issues
- Check WiFi credentials
- Ensure router supports 2.4GHz (ESP32 doesn't support 5GHz)
- Check signal strength

### Display Issues
- Check pin connections
- Check display module configuration
- Ensure correct CS pin selection

### Sensor Issues
- Check DHT11/DHT22 connection
- Verify sensor type matches configuration in `dht22_manager.h` (`DHTTYPE`)
- Check sensor power supply (3.3V or 5V depending on sensor)
- Ensure sensor is not damaged
- For ESP32-C3, use GPIO2 (safer than GPIO12)

### API Issues
- Check API key correctness
- Check API quotas/limits
- Ensure internet connection is stable

## Development

### Code Style
- Follow existing code style
- Use meaningful variable names
- Add comments for complex logic
- Keep functions focused and small

### Testing
- Test on hardware before committing
- Verify all functions work after changes
- Check memory usage

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Support

If you encounter issues or have questions, please open an issue on GitHub.

