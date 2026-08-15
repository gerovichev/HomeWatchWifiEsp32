#pragma once

#include <time.h>  // time_t, used by MIN_VALID_EPOCH

// Timing constants
namespace Timing {
    constexpr int CLOCK_INTERVAL_SEC = 6;
    constexpr int DATA_UPDATE_INTERVAL_SEC = 1200;  // 15 minutes
    constexpr int HTTP_TIMEOUT_MS = 1500;
    constexpr int HTTP_TIMEOUT_CURRENCY_MS = 3000;
    constexpr int OTA_CLIENT_TIMEOUT_MS = 15000;
    constexpr int RETRY_DELAY_MS = 2000;
    constexpr int MQTT_RECONNECT_DELAY_MS = 5000;
    constexpr int NTP_SYNC_WAIT_MS = 500;
    constexpr int SERIAL_INIT_DELAY_MS = 500;
    constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 60000;  // How often loop() polls WiFi state

    // Earliest epoch we accept as "clock is synced". Anything below this means
    // NTP has not landed yet and time-derived logic must not run.
    constexpr time_t MIN_VALID_EPOCH = 946684800;  // 2000-01-01T00:00:00Z
}

// Retry constants
namespace Retry {
    // Sensor/network retry ceilings consumed by ErrorHandler::shouldRetry
    constexpr int MAX_ATTEMPTS_WEATHER = 3;
    constexpr int MAX_ATTEMPTS_LOCATION = 3;
    constexpr int MAX_ATTEMPTS_TIMEZONE = 3;
    constexpr int MAX_ATTEMPTS_MQTT = 3;
    constexpr int MAX_ATTEMPTS_CURRENCY = 3;
}

// Display constants
namespace Display {
    constexpr int SCROLL_SPEED_MS = 50;
    constexpr int PAUSE_TIME_MS = 1000;
    constexpr int INTENSITY_DAY = 2;
    constexpr int INTENSITY_NIGHT = 0;

    // Capacity of Clock::displaySequence, which now holds only the data
    // screens (13 in the fully-featured build) - the clock is interleaved by
    // the rotation itself. buildDisplaySequence() also bounds-checks.
    constexpr int MAX_SEQUENCE_LENGTH = 24;
}

// Buffer sizes
namespace Buffer {
    constexpr size_t LED_BUFFER_SIZE = 512;
    constexpr size_t PATH_BUFFER_SIZE = 512;
    constexpr size_t TIME_STRING_SIZE = 20;
    constexpr size_t TEMP_STRING_SIZE = 8;
    constexpr size_t LOG_BUFFER_SIZE = 192;  // printf-style logging scratch space
}

// Sensor constants
namespace Sensor {
    constexpr int DHT_PIN = 12;
    constexpr float HUMIDITY_DELTA_DEFAULT = 0.00;
}

// Network constants
namespace NetworkConfig {
    constexpr int WIFI_RECONNECT_ATTEMPTS = 20;
    constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
    constexpr int WIFI_INIT_RETRY_ATTEMPTS = 5;  // Количество попыток подключения при старте
    constexpr unsigned long WIFI_INIT_RETRY_DELAY_MS = 5000;  // Задержка между попытками (5 секунд)
    constexpr unsigned long WIFI_INIT_SINGLE_ATTEMPT_TIMEOUT_MS = 15000;  // Таймаут одной попытки (15 секунд)

    // The config portal must not block forever: a device that lost its
    // credentials has to fall back to normal operation (and keep its display
    // alive) instead of sitting in AP mode until someone power-cycles it.
    constexpr int CONFIG_PORTAL_TIMEOUT_SEC = 300;

    // PubSubClient only services the socket once per data-update cycle, so the
    // keepalive has to outlast that interval or the broker drops us every time.
    constexpr uint16_t MQTT_KEEPALIVE_SEC = 2 * Timing::DATA_UPDATE_INTERVAL_SEC;
    constexpr uint16_t MQTT_SOCKET_TIMEOUT_SEC = 15;
}

