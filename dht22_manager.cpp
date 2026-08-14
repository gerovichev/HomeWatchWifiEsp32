#include "dht22_manager.h"
#include "logger.h"
#include "error_handler.h"

Dht22_manager::Dht22_manager() : DHT_Unified(DHTPIN, DHTTYPE){}

// Initializes the DHT22 hardware only (no read) - safe to call from error
// recovery without risking recursion back into a failing read.
void Dht22_manager::initSensor() {
    begin();
    sensor_t sensor;

    // Print temperature sensor details
    temperature().getSensor(&sensor);
    printSensorDetails(sensor, "Temperature");

    // Print humidity sensor details
    humidity().getSensor(&sensor);
    printSensorDetails(sensor, "Humidity");
}

// Function to initialize the DHT22 sensor and set home temperature
void Dht22_manager::dht22Start() {
    if (IS_DHT_CONNECTED) {
        initSensor();
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
        sensorErrorCount = 0;
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

// Function to print detailed sensor information
void Dht22_manager::printSensorDetails(sensor_t sensor, const char* type) {
    LOG_DEBUG_F("------------------------------------");
    LOG_DEBUG(String(type));

    LOG_VERBOSE("Sensor: " + String(sensor.name));
    LOG_VERBOSE("Driver Ver: " + String(sensor.version));
    LOG_VERBOSE("Unique ID: " + String(sensor.sensor_id));

    const char* unit = (strcmp(type, "Temperature") == 0) ? " *C" : " %";

    LOG_VERBOSE("Max Value: " + String(sensor.max_value) + String(unit));
    LOG_VERBOSE("Min Value: " + String(sensor.min_value) + String(unit));
    LOG_VERBOSE("Resolution: " + String(sensor.resolution) + String(unit));
    LOG_DEBUG_F("------------------------------------");
}

// Function to read and print temperature to the display
void Dht22_manager::readAndPrintTemperature() {
    sensors_event_t event;
    temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        handleTemperatureError();
    } else {
        homeTemp = event.temperature;
        sensorErrorCount = 0;
        LOG_VERBOSE("Temperature: " + String(homeTemp, 2) + " *C");
        String tape = String("T") + String(round(homeTemp), 0) + getGradValue() + "C";
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
        sensorErrorCount = 0;
        LOG_VERBOSE("Humidity: " + String(homeHumidity, 2) + "%");
        String tape = String(round(homeHumidity), 0) + "%";
        tape = tape.length() == 4 ? String("H") + tape : String("H ") + tape;
        drawString(tape);
    }
}

// Function to handle temperature reading errors
void Dht22_manager::handleTemperatureError() {
    LOG_ERROR_F("Error reading temperature!");
    if (ErrorHandler::shouldRetry(ErrorHandler::ERROR_SENSOR, sensorErrorCount, 2)) {
        sensorErrorCount++;
        initSensor();  // Reinit hardware only - does not re-read, so this cannot recurse
    } else {
        LOG_WARNING_F("DHT22 temperature errors exceeded retry limit, giving up until next reading");
    }
}

// Function to handle humidity reading errors
void Dht22_manager::handleHumidityError() {
    LOG_ERROR_F("Error reading humidity!");
    if (ErrorHandler::shouldRetry(ErrorHandler::ERROR_SENSOR, sensorErrorCount, 2)) {
        sensorErrorCount++;
        initSensor();  // Reinit hardware only - does not re-read, so this cannot recurse
    } else {
        LOG_WARNING_F("DHT22 humidity errors exceeded retry limit, giving up until next reading");
    }
}
