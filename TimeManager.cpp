#include "TimeManager.h"
#include "secure_client.h"
#include "constants.h"
#include "logger.h"
#include "error_handler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t dataMutex;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

time_t zoneStart;
time_t zoneEnd;
time_t timeNow;

int offset;
String city_name;



void getTimezone() {
  LOG_DEBUG_F("Checking timezone...");
  LOG_VERBOSE("Zone end: " + String(zoneEnd) + ", Current time: " + String(timeNow));
  int maxAttemptsTimes = Retry::MAX_ATTEMPTS_TIMEZONE;

  if (zoneEnd > timeNow) {
    time_t untilTimeMove = zoneEnd - timeNow;
    int daysUntilTimeMove = untilTimeMove / 86400;
    LOG_DEBUG("Days until timezone change: " + String(daysUntilTimeMove));
    LOG_INFO_F("Timezone is current, no update needed");
    return;
  }

  LOG_INFO_F("Fetching timezone information...");
  
  // Optimize URL construction to avoid multiple String concatenations
  char path[Buffer::PATH_BUFFER_SIZE];
  snprintf(path, sizeof(path),
           "https://api.timezonedb.com/v2.1/get-time-zone?key=%s&format=json&lat=%.2f&lng=%.2f&by=position",
           apiKeyTimezone, latitude, longitude);

  // Deliberately logs coordinates only - the full URL carries apiKeyTimezone.
  LOG_DEBUG("Timezone API request for lat=" + String(latitude, 2) +
            ", lon=" + String(longitude, 2));

  int attempts = 0;
  bool success = false;

  while (attempts < maxAttemptsTimes && !success) {
    // A fresh client per attempt: mbedTLS state is not reusable after a failed
    // handshake, so retrying on the same WiFiClientSecure just fails again.
    WiFiClientSecure client;
    setupSecureClient(client, "timezonedb.com");
    HTTPClient http;
    http.setTimeout(Timing::HTTP_TIMEOUT_MS);

    if (http.begin(client, path)) {
      LOG_DEBUG("Timezone API attempt " + String(attempts + 1) + "/" + String(maxAttemptsTimes));
      int httpCode = http.GET();  // Send the request

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        LOG_VERBOSE("Timezone API response: " + payload);

        JsonDocument doc;  // Timezone API response: status, offset, zoneStart, zoneEnd, cityName
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          LOG_VERBOSE_F("Timezone JSON deserialization succeeded");
          JsonObject root = doc.as<JsonObject>();

          // `| ""` matters: an error envelope with no "status" key yields a
          // null const char*, and strcmp() on that faults.
          const char* status = root["status"] | "";
          if (strcmp(status, "OK") == 0) {
            const int newOffset = root["gmtOffset"] | 0;
            const time_t newZoneStart = static_cast<time_t>(root["zoneStart"] | 0L);
            const time_t newZoneEnd = static_cast<time_t>(root["zoneEnd"] | 0L);
            String newCityName = String(root["cityName"] | "");

            // printTimeToScreen() reads timeClient from the display core, so
            // the offset change and its bookkeeping go in one critical section.
            if (dataMutex != NULL) xSemaphoreTake(dataMutex, portMAX_DELAY);
            offset = newOffset;
            timeClient.setTimeOffset(newOffset);
            zoneStart = newZoneStart;
            zoneEnd = newZoneEnd;
            city_name = newCityName;
            if (dataMutex != NULL) xSemaphoreGive(dataMutex);

            LOG_INFO("Timezone updated: " + newCityName + " (UTC" + String(newOffset >= 0 ? "+" : "") + String(newOffset/3600) + ")");
            LOG_DEBUG("GMT offset: " + String(newOffset) + " seconds");

            success = true;
          } else {
            LOG_WARNING("Timezone API returned status: " + String(status));
          }
        } else {
          LOG_ERROR("Timezone JSON deserialization failed: " + String(error.c_str()));
        }
      } else {
        LOG_WARNING("Timezone API HTTP error: " + String(httpCode));
      }
    } else {
      LOG_ERROR_F("Failed to begin timezone HTTP connection");
    }

    http.end();

    if (!success) {
      attempts++;
      if (attempts < maxAttemptsTimes) {
        LOG_WARNING("Retrying timezone request (" + String(attempts) + "/" + String(maxAttemptsTimes) + ")...");
        delay(Timing::RETRY_DELAY_MS);
      } else {
        ErrorHandler::handleError(ErrorHandler::ERROR_API,
                                  "Timezone fetch failed after " + String(maxAttemptsTimes) +
                                      " attempts, keeping current offset",
                                  attempts, maxAttemptsTimes);
      }
    }
  }
}

void printTimeToScreen() {
  String tape = timeClient.getFormattedTime().substring(0, 5);
  drawString(tape);
}

void printDateToScreen() {
  time_t epochTime = timeClient.getEpochTime();
  struct tm timeBuf;
  struct tm* ptm = gmtime_r(&epochTime, &timeBuf);
  if (ptm == nullptr) {
    LOG_WARNING_F("Failed to get time for date display");
    return;
  }
  String tape = getNumberWithZerro(ptm->tm_mday) + F("/") + getNumberWithZerro(ptm->tm_mon + 1);
  drawString(tape);
}

void printDayToScreen() {
  String tape = daysOfTheWeek[timeClient.getDay()];
  drawString(tape);
}

void printCityToScreen() {
  displayTextInSetup(city_name);
}

void ntp_init() {
  LOG_INFO_F("Initializing NTP client...");
  timeClient.begin();
  getTimezone();
  timeClient.update();
  LOG_INFO("NTP synchronized, current time: " + timeClient.getFormattedTime());
  printCityToScreen();
}
