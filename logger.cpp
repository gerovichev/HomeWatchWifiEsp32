#include "logger.h"
#include "constants.h"

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::begin(unsigned long baudRate) {
    if (!isInitialized) {
        Serial.begin(baudRate);
        // Created before isInitialized flips, so no log() call can observe an
        // initialised logger with a null mutex.
        serialMutex = xSemaphoreCreateMutex();
        isInitialized = true;
        delay(Timing::SERIAL_INIT_DELAY_MS); // Give time for Serial to initialize
    }
}

void Logger::setLogLevel(LogLevel level) {
    logLevel = level;
}

LogLevel Logger::getLogLevel() const {
    return logLevel;
}

const char* Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR:   return "[ERROR]  ";
        case LOG_LEVEL_WARNING: return "[WARN]   ";
        case LOG_LEVEL_INFO:    return "[INFO]   ";
        case LOG_LEVEL_DEBUG:   return "[DEBUG]  ";
        case LOG_LEVEL_VERBOSE: return "[VERBOSE]";
        default:                return "[UNKNOWN]";
    }
}

void Logger::log(LogLevel level, const String& message) {
    if (!isInitialized || !Serial || level > logLevel) {
        return;
    }
    
    // Print timestamp (milliseconds since start)
    char timeStr[12];
    snprintf(timeStr, sizeof(timeStr), "%10lu", millis());

    // Hold the mutex across the whole record so a line from the other core
    // cannot be spliced into the middle of this one. A bounded wait keeps a
    // logging problem from ever deadlocking the caller.
    const bool locked =
        (serialMutex != nullptr) &&
        (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE);

    Serial.print(timeStr);
    Serial.print(" ");
    Serial.print(getLevelString(level));
    Serial.print(" ");
    Serial.println(message);

    if (locked) {
        xSemaphoreGive(serialMutex);
    }
}

void Logger::error(const String& message) {
    log(LOG_LEVEL_ERROR, message);
}

void Logger::warning(const String& message) {
    log(LOG_LEVEL_WARNING, message);
}

void Logger::info(const String& message) {
    log(LOG_LEVEL_INFO, message);
}

void Logger::debug(const String& message) {
    log(LOG_LEVEL_DEBUG, message);
}

void Logger::verbose(const String& message) {
    log(LOG_LEVEL_VERBOSE, message);
}

// Filters by level before touching the flash string, so a call below the
// current log level never pays for the String(message) heap allocation.
void Logger::logFlash(LogLevel level, const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || level > logLevel) {
        return;
    }
    log(level, String(message));
}

void Logger::error(const __FlashStringHelper* message) {
    logFlash(LOG_LEVEL_ERROR, message);
}

void Logger::warning(const __FlashStringHelper* message) {
    logFlash(LOG_LEVEL_WARNING, message);
}

void Logger::info(const __FlashStringHelper* message) {
    logFlash(LOG_LEVEL_INFO, message);
}

void Logger::debug(const __FlashStringHelper* message) {
    logFlash(LOG_LEVEL_DEBUG, message);
}

void Logger::verbose(const __FlashStringHelper* message) {
    logFlash(LOG_LEVEL_VERBOSE, message);
}
