#include "weather_manager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Secret.h"
#include "location_manager.h"
#include "secure_client.h"
#include "constants.h"
#include "logger.h"
#include "error_handler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t dataMutex;

WeatherManager::WeatherManager()
{
    // Initialize weather data members here, if necessary
    temperature = 0;
    feelsLikeTemp = 0;
    pressure = 0;
    main_ext_humidity = 0;
    description_weather = "";
}

// Function to read weather data from the OpenWeather API
void WeatherManager::readWeather() {
  LOG_INFO_F("Fetching weather data from OpenWeatherMap...");

  int maxAttempts = Retry::MAX_ATTEMPTS_WEATHER;

  // Optimize URL construction to avoid multiple String concatenations
  char path[Buffer::PATH_BUFFER_SIZE];
  snprintf(path, sizeof(path), 
           "https://api.openweathermap.org/data/3.0/onecall?lat=%.2f&lon=%.2f&units=metric&exclude=minutely,hourly,daily,alerts&appid=%s&lang=%s",
           latitude, longitude, appidWeather, lang_weather.c_str());

  // Deliberately logs coordinates only - the full URL carries appidWeather.
  LOG_DEBUG("Weather API request for lat=" + String(latitude, 2) +
            ", lon=" + String(longitude, 2) + ", lang=" + lang_weather);

  int attempts = 0;
  bool success = false;

  while (attempts < maxAttempts && !success) {
    // A fresh client per attempt: mbedTLS state is not reusable after a failed
    // handshake, so retrying on the same WiFiClientSecure just fails again.
    WiFiClientSecure client;
    setupSecureClient(client, "openweathermap.org");
    HTTPClient http;
    http.setTimeout(Timing::HTTP_TIMEOUT_MS);

    if (http.begin(client, path)) {
      LOG_DEBUG("Weather API attempt " + String(attempts + 1) + "/" + String(maxAttempts));
      int httpCode = http.GET();  // Send the request

      if (httpCode == HTTP_CODE_OK) {  // Check the returning code
        String payload = http.getString();  // Get the request response payload
        LOG_VERBOSE("Weather API response: " + payload);

        JsonDocument doc;  // Weather API response with current weather data
        DeserializationError error = deserializeJson(doc, payload);

        // Test if parsing succeeds
        if (!error) {
          JsonObject current = doc[F("current")];

          // Signed: offsets west of UTC are negative, and an unsigned type here
          // wraps them into ~4.29e9 and destroys the sunrise/sunset window.
          long timezone_offset = doc[F("timezone_offset")] | 0L;

          // Compute everything into locals first, so the critical section is a
          // single burst of assignments and the display core can never observe
          // sunrise before its timezone adjustment is applied.
          const time_t newSunrise =
              static_cast<time_t>(current[F("sunrise")] | 0L) + timezone_offset;
          const time_t newSunset =
              static_cast<time_t>(current[F("sunset")] | 0L) + timezone_offset;

          const int newTemperature = (int)floor((double)current[F("temp")] + 0.5);
          const int newFeelsLike = (int)floor((double)current[F("feels_like")] + 0.5);
          const int newPressure = (int)((double)current[F("pressure")] * 0.75006375541921);  // Convert pressure to mmHg
          const int newHumidity = current[F("humidity")] | 0;

          // `| ""` keeps a missing/!string description from yielding "null".
          String newDescription = String(current[F("weather")][0][F("description")] | "");
          newDescription.toUpperCase();

          if (dataMutex != NULL) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
          }

          sunrise = newSunrise;
          sunset = newSunset;
          temperature = newTemperature;
          feelsLikeTemp = newFeelsLike;
          pressure = newPressure;
          main_ext_humidity = newHumidity;
          description_weather = newDescription;

          if (dataMutex != NULL) {
            xSemaphoreGive(dataMutex);
          }

          LOG_INFO("Weather updated: " + String(newTemperature) + "°C, " +
                   String(newHumidity) + "%, " + String(newPressure) + "mm");
          LOG_DEBUG("Feels like: " + String(newFeelsLike) + "°C");
          LOG_DEBUG("Description: " + newDescription);

          success = true;  // Set success flag
        } else {
          LOG_ERROR("Weather JSON deserialization failed: " + String(error.c_str()));
        }
      } else {
        LOG_WARNING("Weather API HTTP error: " + String(httpCode));
      }

    } else {
      LOG_ERROR_F("Failed to begin weather HTTP connection");
    }

    http.end();  // Close connection before the next retry reuses this client

    if (!success) {
      attempts++;
      if (attempts < maxAttempts) {
        LOG_WARNING("Retrying weather request (" + String(attempts) + "/" + String(maxAttempts) + ")...");
        delay(Timing::RETRY_DELAY_MS);  // Wait before retrying
      } else {
        ErrorHandler::handleError(ErrorHandler::ERROR_API,
                                  "Weather fetch failed after " + String(maxAttempts) +
                                      " attempts, keeping previous reading",
                                  attempts, maxAttempts);
      }
    }
  }
}

// Function to print temperature on the screen
void WeatherManager::printWeatherToScreen() const{
  String tape = String(temperature, DEC) + getGradValue() + String("C");
  drawString(tape);
}

// Function to print feels-like temperature on the screen
void WeatherManager::printFeelsLikeToScreen() const{
  // Very short format to fit on display: "~25°C" (tilde ~ means "feels like")
  String tape = String("~") + String(feelsLikeTemp, DEC) + getGradValue() + String("C");
  drawString(tape);
}

// Function to print pressure on the screen
void WeatherManager::printPressureToScreen() const{
  String tape = String(pressure, DEC) + String("mm");
  drawString(tape);
}

// Function to print humidity on the screen
void WeatherManager::printHumidityToScreen() const{
  String tape = String(main_ext_humidity, DEC) + String("%");
  drawString(tape);
}

// Function to print weather description on the screen
void WeatherManager::printDescriptionWeatherToScreen() const {  
  drawString(description_weather);
}
