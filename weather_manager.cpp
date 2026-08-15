#include "weather_manager.h"
#include <ArduinoJson.h>
#include "Secret.h"
#include "location_manager.h"
#include "constants.h"
#include "logger.h"
#include "http_fetch.h"
#include "data_lock.h"
#include "device_state.h"

WeatherManager::WeatherManager() = default;

// Function to read weather data from the OpenWeather API
void WeatherManager::readWeather() {
  LOG_INFO_F("Fetching weather data from OpenWeatherMap...");

  const String &language = DeviceState::getInstance().getLanguage();

  // Optimize URL construction to avoid multiple String concatenations
  char path[Buffer::PATH_BUFFER_SIZE];
  snprintf(path, sizeof(path),
           "https://api.openweathermap.org/data/3.0/onecall?lat=%.2f&lon=%.2f&units=metric&exclude=minutely,hourly,daily,alerts&appid=%s&lang=%s",
           latitude, longitude, appidWeather, language.c_str());

  // Deliberately logs coordinates only - the full URL carries appidWeather.
  LOG_DEBUGF("Weather API request for lat=%.2f, lon=%.2f, lang=%s", latitude,
             longitude, language.c_str());

  HttpFetchOptions opts;
  opts.url = path;
  opts.tag = "openweathermap.org";
  opts.timeoutMs = Timing::HTTP_TIMEOUT_MS;
  opts.maxAttempts = Retry::MAX_ATTEMPTS_WEATHER;

  httpFetchWithRetry(opts, [this](const String &payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      LOG_ERROR("Weather JSON deserialization failed: " + String(error.c_str()));
      return false;
    }

    JsonObject current = doc[F("current")];

    // Signed: offsets west of UTC are negative, and an unsigned type here
    // wraps them into ~4.29e9 and destroys the sunrise/sunset window.
    const long timezone_offset = doc[F("timezone_offset")] | 0L;

    // Everything is computed into locals first so the critical section below
    // is a single burst of assignments - the display core can never observe
    // sunrise before its timezone adjustment is applied.
    const time_t newSunrise =
        static_cast<time_t>(current[F("sunrise")] | 0L) + timezone_offset;
    const time_t newSunset =
        static_cast<time_t>(current[F("sunset")] | 0L) + timezone_offset;

    WeatherSnapshot fresh;
    fresh.temperature = (int)floor((double)current[F("temp")] + 0.5);
    fresh.feelsLike = (int)floor((double)current[F("feels_like")] + 0.5);
    fresh.pressure = (int)((double)current[F("pressure")] * 0.75006375541921);  // Convert pressure to mmHg
    fresh.humidity = current[F("humidity")] | 0;

    // `| ""` keeps a missing/non-string description from yielding "null".
    String newDescription = String(current[F("weather")][0][F("description")] | "");
    newDescription.toUpperCase();
    strncpy(fresh.description, newDescription.c_str(), sizeof(fresh.description) - 1);
    fresh.description[sizeof(fresh.description) - 1] = '\0';

    {
      DataLock lock;
      data = fresh;
      sunrise = newSunrise;
      sunset = newSunset;
    }

    LOG_INFOF("Weather updated: %d\xB0""C, %d%%, %dmm (feels like %d\xB0""C)",
              fresh.temperature, fresh.humidity, fresh.pressure, fresh.feelsLike);
    LOG_DEBUGF("Description: %s", fresh.description);
    return true;
  });
}

WeatherSnapshot WeatherManager::snapshot() const {
  DataLock lock;
  return data;
}

// Function to print temperature on the screen
void WeatherManager::printWeatherToScreen() const{
  const WeatherSnapshot s = snapshot();
  drawString(String(s.temperature, DEC) + getGradValue() + String("C"));
}

// Function to print feels-like temperature on the screen
void WeatherManager::printFeelsLikeToScreen() const{
  // Very short format to fit on display: "~25°C" (tilde ~ means "feels like")
  const WeatherSnapshot s = snapshot();
  drawString(String("~") + String(s.feelsLike, DEC) + getGradValue() + String("C"));
}

// Function to print pressure on the screen
void WeatherManager::printPressureToScreen() const{
  const WeatherSnapshot s = snapshot();
  drawString(String(s.pressure, DEC) + String("mm"));
}

// Function to print humidity on the screen
void WeatherManager::printHumidityToScreen() const{
  const WeatherSnapshot s = snapshot();
  drawString(String(s.humidity, DEC) + String("%"));
}

// Function to print weather description on the screen
void WeatherManager::printDescriptionWeatherToScreen() const {
  const WeatherSnapshot s = snapshot();
  drawString(String(s.description));
}
