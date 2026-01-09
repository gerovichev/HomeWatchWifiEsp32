#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Define to disable verbose/debug logging to reduce code size
#define DISABLE_VERBOSE_LOGGING
#define DISABLE_DEBUG_LOGGING

// Log levels
enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_VERBOSE = 5
};

class Logger {
public:
    static Logger& getInstance();
    
    void begin(unsigned long baudRate = 115200);
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    
    // Flash string methods (preferred - saves RAM)
    void error(const __FlashStringHelper* message);
    void warning(const __FlashStringHelper* message);
    void info(const __FlashStringHelper* message);
    #ifndef DISABLE_DEBUG_LOGGING
    void debug(const __FlashStringHelper* message);
    #endif
    #ifndef DISABLE_VERBOSE_LOGGING
    void verbose(const __FlashStringHelper* message);
    #endif
    
    // Simplified String methods - minimal implementation to save code size
    void error(const String& message);
    void warning(const String& message);
    void info(const String& message);

private:
    Logger() : logLevel(LOG_LEVEL_INFO), isInitialized(false) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    LogLevel logLevel;
    bool isInitialized;
};

// Convenience macros for logging - support both Flash strings and String objects
// These macros will use String methods when String concatenation is used
#define LOG_ERROR(msg) Logger::getInstance().error(String(msg))
#define LOG_WARNING(msg) Logger::getInstance().warning(String(msg))
#define LOG_INFO(msg) Logger::getInstance().info(String(msg))
#ifdef DISABLE_DEBUG_LOGGING
#define LOG_DEBUG(msg) ((void)0)
#define LOG_DEBUG_F(msg) ((void)0)
#else
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_DEBUG_F(msg) Logger::getInstance().debug(F(msg))
#endif
#ifdef DISABLE_VERBOSE_LOGGING
#define LOG_VERBOSE(msg) ((void)0)
#define LOG_VERBOSE_F(msg) ((void)0)
#else
#define LOG_VERBOSE(msg) Logger::getInstance().verbose(msg)
#define LOG_VERBOSE_F(msg) Logger::getInstance().verbose(F(msg))
#endif

// Macros with Flash strings
#define LOG_ERROR_F(msg) Logger::getInstance().error(F(msg))
#define LOG_WARNING_F(msg) Logger::getInstance().warning(F(msg))
#define LOG_INFO_F(msg) Logger::getInstance().info(F(msg))

// Optimized macros for logging with variables - use Serial.print() directly to avoid String concatenation
// These macros print directly without creating String objects, saving RAM
#define LOG_INFO_VAR(prefix, var) do { \
    if (Logger::getInstance().getLogLevel() >= LOG_LEVEL_INFO && Serial) { \
        Serial.print(F("[INFO]   ")); \
        Serial.print(F(" ")); \
        Serial.print(F(prefix)); \
        Serial.println(var); \
    } \
} while(0)

#define LOG_WARNING_VAR(prefix, var) do { \
    if (Logger::getInstance().getLogLevel() >= LOG_LEVEL_WARNING && Serial) { \
        Serial.print(F("[WARN]   ")); \
        Serial.print(F(" ")); \
        Serial.print(F(prefix)); \
        Serial.println(var); \
    } \
} while(0)

#define LOG_ERROR_VAR(prefix, var) do { \
    if (Logger::getInstance().getLogLevel() >= LOG_LEVEL_ERROR && Serial) { \
        Serial.print(F("[ERROR]  ")); \
        Serial.print(F(" ")); \
        Serial.print(F(prefix)); \
        Serial.println(var); \
    } \
} while(0)

#ifndef DISABLE_DEBUG_LOGGING
#define LOG_DEBUG_VAR(prefix, var) do { \
    if (Logger::getInstance().getLogLevel() >= LOG_LEVEL_DEBUG && Serial) { \
        Serial.print(F("[DEBUG]  ")); \
        Serial.print(F(" ")); \
        Serial.print(F(prefix)); \
        Serial.println(var); \
    } \
} while(0)
#else
#define LOG_DEBUG_VAR(prefix, var) ((void)0)
#endif

#endif // LOGGER_H
