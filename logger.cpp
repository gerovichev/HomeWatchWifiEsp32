#include "logger.h"
#include "constants.h"

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::begin(unsigned long baudRate) {
    if (!isInitialized) {
        Serial.begin(baudRate);
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

// Simplified String methods - minimal implementation
void Logger::error(const String& message) {
    if (!isInitialized || !Serial || LOG_LEVEL_ERROR > logLevel) return;
    Serial.print(F("[ERROR]  "));
    Serial.print(F(" "));
    Serial.println(message);
}

void Logger::warning(const String& message) {
    if (!isInitialized || !Serial || LOG_LEVEL_WARNING > logLevel) return;
    Serial.print(F("[WARN]   "));
    Serial.print(F(" "));
    Serial.println(message);
}

void Logger::info(const String& message) {
    if (!isInitialized || !Serial || LOG_LEVEL_INFO > logLevel) return;
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.println(message);
}

// Optimized Flash string handlers - print directly without String conversion
void Logger::error(const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || LOG_LEVEL_ERROR > logLevel) {
        return;
    }
    Serial.print(F("[ERROR]  "));
    Serial.print(F(" "));
    Serial.println(message);
}

void Logger::warning(const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || LOG_LEVEL_WARNING > logLevel) {
        return;
    }
    Serial.print(F("[WARN]   "));
    Serial.print(F(" "));
    Serial.println(message);
}

void Logger::info(const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || LOG_LEVEL_INFO > logLevel) {
        return;
    }
    Serial.print(F("[INFO]   "));
    Serial.print(F(" "));
    Serial.println(message);
}

#ifndef DISABLE_DEBUG_LOGGING
void Logger::debug(const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || LOG_LEVEL_DEBUG > logLevel) {
        return;
    }
    Serial.print(F("[DEBUG]  "));
    Serial.print(F(" "));
    Serial.println(message);
}
#endif

#ifndef DISABLE_VERBOSE_LOGGING
void Logger::verbose(const __FlashStringHelper* message) {
    if (!isInitialized || !Serial || LOG_LEVEL_VERBOSE > logLevel) {
        return;
    }
    Serial.print(F("[VERBOSE]"));
    Serial.print(F(" "));
    Serial.println(message);
}
#endif
