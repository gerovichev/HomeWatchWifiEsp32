#include "dht22_manager.h"
#include "logger.h"
#include "error_handler.h"
#include "data_lock.h"
#include "device_state.h"

Dht22_manager::Dht22_manager() : dht(DHTPIN, DHTTYPE) {}

// Initializes the DHT22 hardware only (no read) - safe to call from error
// recovery without risking recursion back into a failing read.
void Dht22_manager::initSensor() {
    dht.begin();
    sensor_t sensor;

    // Print temperature sensor details
    dht.temperature().getSensor(&sensor);
    printSensorDetails(sensor, "Temperature");

    // Print humidity sensor details
    dht.humidity().getSensor(&sensor);
    printSensorDetails(sensor, "Humidity");
}

// Function to initialize the DHT22 sensor and take the first reading
void Dht22_manager::dht22Start() {
    if (DeviceState::getInstance().isDhtConnected()) {
        initSensor();
        refresh();
    }
}

// Blocking sensor read. Runs on the data-update task; the values land in the
// cache under dataMutex so the display core can read them without blocking.
void Dht22_manager::refresh() {
    if (!DeviceState::getInstance().isDhtConnected()) {
        return;
    }

    sensors_event_t tempEvent;
    sensors_event_t humidityEvent;
    dht.temperature().getEvent(&tempEvent);
    dht.humidity().getEvent(&humidityEvent);

    const bool tempOk = !isnan(tempEvent.temperature);
    const bool humidityOk = !isnan(humidityEvent.relative_humidity);

    if (!tempOk) {
        handleSensorError(F("temperature"));
    }
    if (!humidityOk) {
        handleSensorError(F("humidity"));
    }
    if (!tempOk && !humidityOk) {
        return;
    }

    sensorErrorCount = 0;

    {
        DataLock lock;
        if (tempOk) {
            homeTemp = tempEvent.temperature;
        }
        if (humidityOk) {
            homeHumidity = humidityEvent.relative_humidity +
                           DeviceState::getInstance().getHumidityDelta();
        }
    }

    if (tempOk) {
        LOG_VERBOSEF("Temperature: %.2f *C", tempEvent.temperature);
    }
    if (humidityOk) {
        LOG_VERBOSEF("Humidity: %.2f%%", humidityEvent.relative_humidity +
                                             DeviceState::getInstance().getHumidityDelta());
    }
}

float Dht22_manager::getHomeTemp() const {
    DataLock lock;
    return homeTemp;
}

// Function to print home temperature to the display
void Dht22_manager::printHomeTemp() {
    float value;
    {
        DataLock lock;
        value = homeTemp;
    }

    if (isnan(value)) {
        LOG_DEBUG_F("No temperature reading available yet");
        return;
    }

    drawString(String("T") + String(round(value), 0) + getGradValue() + "C");
}

// Function to print humidity to the display
void Dht22_manager::printHumidity() {
    float value;
    {
        DataLock lock;
        value = homeHumidity;
    }

    if (isnan(value)) {
        LOG_DEBUG_F("No humidity reading available yet");
        return;
    }

    String tape = String(round(value), 0) + "%";
    // "H100%" fills the panel; anything shorter gets a space so it lines up.
    drawString(tape.length() == 4 ? String("H") + tape : String("H ") + tape);
}

// Function to print detailed sensor information
void Dht22_manager::printSensorDetails(const sensor_t& sensor, const char* type) {
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

// Shared failure path for both channels
void Dht22_manager::handleSensorError(const __FlashStringHelper* what) {
    LOG_ERROR("Error reading " + String(what) + " from DHT22");

    if (ErrorHandler::shouldRetry(ErrorHandler::ERROR_SENSOR, sensorErrorCount, 2)) {
        sensorErrorCount++;
        initSensor();  // Reinit hardware only - does not re-read, so this cannot recurse
    } else {
        LOG_WARNING_F("DHT22 errors exceeded retry limit, giving up until next cycle");
    }
}
