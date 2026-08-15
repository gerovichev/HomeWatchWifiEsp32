#include "logger.h"
#include "constants.h"
#include <stdarg.h>

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

// Emits one already-formatted record. log() and logf() both funnel through
// here so the mutex discipline lives in exactly one place.
void Logger::writeRecord(LogLevel level, const char* message) {
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

void Logger::log(LogLevel level, const String& message) {
    if (!isInitialized || !Serial || level > logLevel) {
        return;
    }
    writeRecord(level, message.c_str());
}

void Logger::logf(LogLevel level, const char* format, ...) {
    // Level check first: a filtered-out call must not pay for formatting.
    if (!isInitialized || !Serial || level > logLevel) {
        return;
    }

    char buffer[Buffer::LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (written < 0) {
        writeRecord(LOG_LEVEL_ERROR, "log formatting failed");
        return;
    }

    // vsnprintf truncates rather than overflowing; mark it so a clipped record
    // is not mistaken for the whole story.
    if (static_cast<size_t>(written) >= sizeof(buffer)) {
        buffer[sizeof(buffer) - 4] = '.';
        buffer[sizeof(buffer) - 3] = '.';
        buffer[sizeof(buffer) - 2] = '.';
        buffer[sizeof(buffer) - 1] = '\0';
    }

    writeRecord(level, buffer);
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
