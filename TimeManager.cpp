#include "TimeManager.h"
#include "constants.h"
#include "logger.h"
#include "http_fetch.h"
#include "data_lock.h"

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

  HttpFetchOptions opts;
  opts.url = path;
  opts.tag = "timezonedb.com";
  opts.timeoutMs = Timing::HTTP_TIMEOUT_MS;
  opts.maxAttempts = maxAttemptsTimes;

  httpFetchWithRetry(opts, [](const String &payload) {
    JsonDocument doc;  // Timezone API response: status, offset, zoneStart, zoneEnd, cityName
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      LOG_ERROR("Timezone JSON deserialization failed: " + String(error.c_str()));
      return false;
    }

    JsonObject root = doc.as<JsonObject>();

    // `| ""` matters: an error envelope with no "status" key yields a
    // null const char*, and strcmp() on that faults.
    const char *status = root["status"] | "";
    if (strcmp(status, "OK") != 0) {
      LOG_WARNING("Timezone API returned status: " + String(status));
      return false;
    }

    const int newOffset = root["gmtOffset"] | 0;
    const time_t newZoneStart = static_cast<time_t>(root["zoneStart"] | 0L);
    const time_t newZoneEnd = static_cast<time_t>(root["zoneEnd"] | 0L);
    String newCityName = String(root["cityName"] | "");

    {
      // printTimeToScreen() reads timeClient from the display core, so the
      // offset change and its bookkeeping go in one critical section.
      DataLock lock;
      offset = newOffset;
      timeClient.setTimeOffset(newOffset);
      zoneStart = newZoneStart;
      zoneEnd = newZoneEnd;
      city_name = newCityName;
    }

    LOG_INFO("Timezone updated: " + newCityName + " (UTC" + String(newOffset >= 0 ? "+" : "") + String(newOffset/3600) + ")");
    LOG_DEBUG("GMT offset: " + String(newOffset) + " seconds");
    return true;
  });
}

// The three screens below all read timeClient, which getTimezone() mutates from
// the network core. Each takes the lock only for the read, then formats and
// draws outside the critical section.
time_t currentEpoch() {
  DataLock lock;
  return timeClient.getEpochTime();
}

void printTimeToScreen() {
  String formatted;
  {
    DataLock lock;
    formatted = timeClient.getFormattedTime();
  }
  drawString(formatted.substring(0, 5));
}

void printDateToScreen() {
  time_t epochTime = currentEpoch();
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
  int dayIndex;
  {
    DataLock lock;
    dayIndex = timeClient.getDay();
  }
  if (dayIndex < 0 || dayIndex > 6) {
    LOG_WARNING("Day index out of range: " + String(dayIndex));
    return;
  }
  drawString(daysOfTheWeek[dayIndex]);
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
