# HomeWatchWifi ESP32

A smart watch project based on ESP32 with Max72xxPanel LED display that shows time, weather, currency rates, and environmental data.

![License](https://img.shields.io/badge/license-GPLv3-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)

## Features

- **Time & Date Display**: Real-time clock with NTP synchronization
- **Weather Information**: Current weather, temperature, humidity, pressure from OpenWeatherMap API
- **Currency Rates**: USD and EUR exchange rates
- **Environmental Sensors**: DHT22 temperature and humidity sensor support
- **Location Services**: Automatic location detection via Google Geolocation API
- **Multi-language Support**: Russian and English interface
- **Energy Efficient**: WiFi power management and automatic brightness control
- **OTA Updates**: Over-the-air firmware updates
- **MQTT Integration**: Temperature data publishing to MQTT broker
- **Multi-device Support**: Configuration for multiple devices via MAC addresses
- **Centralized Logging**: Unified logging system with configurable levels
- **Error Handling**: Robust error handling with graceful degradation

## Hardware Requirements

- ESP32 microcontroller
- Max72xxPanel LED matrix display (4 modules)
- DHT22 temperature/humidity sensor (optional)
- WiFi connection

## Pin Configuration

- **CLK_PIN**: 14
- **DATA_PIN**: 13
- **CS_PIN**: 5
- **DHTPIN**: 12

## Prerequisites

- Arduino IDE or PlatformIO
- ESP32 board support package
- Required libraries (see below)

## Required Libraries

- `WiFi` (included with ESP32 board package)
- `HTTPClient` (included with ESP32 board package)
- `WiFiClientSecure` (included with ESP32 board package)
- `ArduinoJson` (for JSON parsing)
- `MD_MAX72XX` (for LED matrix display)
- `MD_Parola` (for text scrolling on display)
- `TimeLib` (for time management)
- `DHT` (for DHT22 sensor)
- `Ticker` (for periodic tasks)
- `WiFiManager` (for WiFi captive portal)

## Setup Instructions

1. **Open the project**
   - Open `HomeWatchWifiEsp32.ino` in Arduino IDE

2. **Configure API Keys**: Create `Secret.cpp` based on `Secret.h` template with your API credentials.

   **Important**: 
   - `Secret.cpp` is not tracked by git (it's in `.gitignore`)
   - `Secret.h` is the template file and is safe to publish
   - Never commit your real API keys to the repository
   - To find your device's MAC address, check the Serial monitor during first boot

3. **Device Configuration**: 
   - Modify device configurations in `Secret.cpp` for your specific devices
   - Add your device's MAC address and settings to the `setDeviceConfig()` function

4. **Upload to ESP32**: 
   - Select your ESP32 board in Arduino IDE
   - Select the correct COM port
   - Compile and upload the code

5. **WiFi Setup**: 
   - Connect to the AutoConnectAP network
   - Configure your WiFi credentials through the web interface

## API Services Used

- **OpenWeatherMap**: Weather data
- **Google Geolocation API**: Location services
- **TimezoneDB**: Timezone information
- **Custom Currency API**: Exchange rates
- **NTP Servers**: Time synchronization

## Display Cycle

The device cycles through the following information every 5 seconds:
1. Current time
2. Date
3. Day of week
4. Current temperature
5. Feels-like temperature
6. Atmospheric pressure
7. Humidity
8. Weather description
9. USD exchange rate
10. EUR exchange rate
11. Home temperature (DHT22)
12. Home humidity (DHT22)
13. Calendar events (if configured)

## Project Structure

```
HomeWatchWifiEsp32/
├── HomeWatchWifiEsp32.ino  # Main entry point
├── main_process.cpp/h      # Main process logic
├── clock.cpp/h             # Clock and display management
├── weather_manager.cpp/h   # Weather data handling
├── currency_manager.cpp/h  # Currency rates handling
├── location_manager.cpp/h  # Location services
├── TimeManager.cpp/h       # Time synchronization
├── dht22_manager.cpp/h     # DHT22 sensor management
├── led_display.cpp/h       # LED display functions
├── logger.cpp/h            # Logging system
├── error_handler.cpp/h     # Error handling
├── device_state.cpp/h      # Device state management
├── constants.h             # Centralized constants
├── secure_client.h         # SSL client configuration
├── global_config.cpp/h     # Global configuration
├── WiFiSetup.cpp/h         # WiFi setup
├── OTAUpdate.cpp/h         # OTA updates
├── MQTTClient.cpp/h        # MQTT client
└── fonts.h                 # Font definitions
```

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
