#include "dht22_manager.h"
// #include "board_led.h"  // Disabled - board LED not used
#include "logger.h"

// Define global variables
float homeTemp = 0.0;
float homeHumidity = 0.0;

Dht22_manager::Dht22_manager() : DHT_Unified(DHTPIN, DHTTYPE){}

// Function to initialize the DHT22 sensor and set home temperature
void Dht22_manager::dht22Start() {
    if (IS_DHT_CONNECTED) {
        begin();
        sensor_t sensor;

        // Print temperature sensor details
        temperature().getSensor(&sensor);
        printSensorDetails(sensor, "Temperature");

        // Print humidity sensor details
        humidity().getSensor(&sensor);
        printSensorDetails(sensor, "Humidity");

        setHomeTemp();  // Read and set initial home temperature
    }
}

// Function to read and set the home temperature
void Dht22_manager::setHomeTemp() {
    sensors_event_t event;
    temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        handleTemperatureError();
    } else {
        homeTemp = event.temperature;
    }
}

float Dht22_manager::getHomeTemp()
{
  return homeTemp;
}

// Function to print home temperature to the display
void Dht22_manager::printHomeTemp() {
    readAndPrintTemperature();
}

// Function to print humidity to the display
void Dht22_manager::printHumidity() {
    readAndPrintHumidity();
}

// Function to print detailed sensor information - simplified to reduce code size
void Dht22_manager::printSensorDetails(sensor_t sensor, const char* type) {
    // Simplified - removed verbose logging to reduce code size
    LOG_INFO(String(type) + " sensor initialized");
}

// Function to read and print temperature to the display
void Dht22_manager::readAndPrintTemperature() {
    sensors_event_t event;
    temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        handleTemperatureError();
    } else {
        homeTemp = event.temperature;
        LOG_INFO("Home temperature: " + String(homeTemp, 2) + "°C");
        String tape = String("T") + String(round(homeTemp), 0) + getGradValue() + "C";
        LOG_INFO(">> Display: Home Temp = " + tape);
        // changeLEDColorForDisplay(DISPLAY_HOME_TEMP);  // Disabled - board LED not used
        drawString(tape);
    }
}

// Function to read and print humidity to the display
void Dht22_manager::readAndPrintHumidity() {
    sensors_event_t event;
    humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        handleHumidityError();
    } else {
        homeHumidity = event.relative_humidity + humidity_delta;
        LOG_INFO("Home humidity: " + String(homeHumidity, 2) + "%");
        String tape = String(round(homeHumidity), 0) + "%";
        tape = tape.length() == 4 ? String("H") + tape : String("H ") + tape;
        LOG_INFO(">> Display: Home Humidity = " + tape);
        // changeLEDColorForDisplay(DISPLAY_HOME_HUMIDITY);  // Disabled - board LED not used
        drawString(tape);
    }
}

// Function to handle temperature reading errors
void Dht22_manager::handleTemperatureError() {
    dht22Start();  // Restart the DHT sensor
    LOG_ERROR_F("Error reading temperature!");
}

// Function to handle humidity reading errors
void Dht22_manager::handleHumidityError() {
    dht22Start();  // Restart the DHT sensor
    LOG_ERROR_F("Error reading humidity!");
}

