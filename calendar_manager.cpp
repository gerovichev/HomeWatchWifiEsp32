#include "calendar_manager.h"
#include "constants.h"
#include "logger.h"
#include "TimeManager.h"
#include "global_config.h"
#include "led_display.h"
#include "device_state.h"
#include "data_lock.h"
#include "text_utils.h"
#include <TimeLib.h>
#include "clock.h"

namespace {
constexpr unsigned long DISPLAY_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr int SECONDS_PER_DAY = 86400;
constexpr int MAX_DAYS_AHEAD = 365;

// Copies a String into a fixed char buffer, always NUL-terminated.
void storeText(char* dest, size_t size, const String& src) {
    strncpy(dest, src.c_str(), size - 1);
    dest[size - 1] = '\0';
}

bool isGeneric(const BirthdayEvent& event) {
    return event.hostname == nullptr || strlen(event.hostname) == 0;
}
} // namespace

CalendarManager::CalendarManager() : lastUpdateDay(-1), lastDisplayTime(0) {}

// Check if event matches current board hostname
bool CalendarManager::matchesHostname(const char* eventHostname) const {
    // Empty string means event applies to all boards
    if (eventHostname == nullptr || strlen(eventHostname) == 0) {
        return true;
    }
    return DeviceState::getInstance().getHostname() == String(eventHostname);
}

// Find next upcoming event
void CalendarManager::findNextEvent() {
    {
        DataLock lock;
        data = CalendarSnapshot();
    }

    if (calendarEventsCount == 0) {
        return;
    }

    const time_t now = currentEpoch();
    if (now < Timing::MIN_VALID_EPOCH) {
        return; // Time not synchronized
    }

    struct tm timeinfoBuf;
    struct tm* timeinfo = gmtime_r(&now, &timeinfoBuf);
    if (timeinfo == nullptr) {
        return;
    }

    const int currentMonth = timeinfo->tm_mon + 1; // tm_mon is 0-11
    const int currentDay = timeinfo->tm_mday;
    const int currentHour = timeinfo->tm_hour;
    const int currentYear = timeinfo->tm_year + 1900;

    const String currentHostname = DeviceState::getInstance().getHostname();

    // One pre-pass marking the dates that have a board-specific event, so the
    // generic-vs-specific check below is a bit test instead of a nested scan
    // over every event. Bit d of specificDays[m] means month m, day d.
    uint32_t specificDays[13] = {0};
    for (int i = 0; i < calendarEventsCount; i++) {
        const BirthdayEvent& event = calendarEvents[i];
        if (isGeneric(event) || currentHostname != String(event.hostname)) {
            continue;
        }
        if (event.month >= 1 && event.month <= 12 && event.day >= 1 && event.day <= 31) {
            specificDays[event.month] |= (1UL << event.day);
        }
    }

    const BirthdayEvent* nextEvent = nullptr;
    time_t nextEventStartTimeValue = 0;
    time_t nextEventEndTimeValue = 0;
    int minDaysAhead = MAX_DAYS_AHEAD + 1;

    for (int i = 0; i < calendarEventsCount; i++) {
        const BirthdayEvent* event = &calendarEvents[i];

        // Skip events that don't match this board's hostname
        if (!matchesHostname(event->hostname)) {
            continue;
        }

        // A generic ("all boards") event yields to a board-specific event on
        // the same date: "for all boards EXCEPT those with their own entry".
        if (isGeneric(*event) &&
            event->month >= 1 && event->month <= 12 &&
            (specificDays[event->month] & (1UL << event->day))) {
            continue;
        }

        const bool isToday = (event->month == currentMonth && event->day == currentDay);
        bool isPastToday = false;

        // Check if event is today but already passed
        if (isToday && event->fromHour >= 0) {
            if (event->toHour >= 0) {
                isPastToday = (currentHour > event->toHour);
            } else {
                // Start time but no end time: treat as an hour long.
                isPastToday = (currentHour > event->fromHour + 1);
            }
        }
        // All-day events stay valid until 23:59, so they are never past today.

        if (isToday && isPastToday) {
            continue;
        }

        // Calculate event start time
        struct tm eventStartTm = {0};
        if (event->month > currentMonth ||
            (event->month == currentMonth && event->day > currentDay) ||
            isToday) {
            // Event is this year (today or future)
            eventStartTm.tm_year = currentYear - 1900;
        } else {
            // Event is next year
            eventStartTm.tm_year = currentYear - 1900 + 1;
        }
        eventStartTm.tm_mon = event->month - 1;
        eventStartTm.tm_mday = event->day;
        eventStartTm.tm_hour = (event->fromHour >= 0) ? event->fromHour : 0;
        eventStartTm.tm_min = 0;
        eventStartTm.tm_sec = 0;

        // mktime interprets struct tm as local time; setClock() calls
        // configTime(0, 0, ...) so the process TZ is UTC, matching the
        // gmtime_r() used to decompose `now` above.
        const time_t eventStartTime = mktime(&eventStartTm);

        struct tm eventEndTm = eventStartTm;
        if (event->toHour >= 0) {
            eventEndTm.tm_hour = event->toHour;
            eventEndTm.tm_min = 59;
        } else {
            // All-day event ends at 23:59
            eventEndTm.tm_hour = 23;
            eventEndTm.tm_min = 59;
        }
        const time_t eventEndTime = mktime(&eventEndTm);

        const int daysUntilEvent = (eventStartTime - now) / SECONDS_PER_DAY;

        if (daysUntilEvent >= 0 && daysUntilEvent < minDaysAhead) {
            minDaysAhead = daysUntilEvent;
            nextEvent = event;
            nextEventStartTimeValue = eventStartTime;
            nextEventEndTimeValue = eventEndTime;
        }
    }

    if (nextEvent == nullptr || minDaysAhead > MAX_DAYS_AHEAD) {
        return;
    }

    // Build the snapshot outside the lock, then publish it in one assignment.
    CalendarSnapshot fresh;
    fresh.hasEvent = true;
    fresh.startTime = nextEventStartTimeValue;
    fresh.endTime = nextEventEndTimeValue;

    const bool hasBoardTitle =
        nextEvent->boardTitle != nullptr && strlen(nextEvent->boardTitle) > 0;
    storeText(fresh.title, sizeof(fresh.title),
              String(hasBoardTitle ? nextEvent->boardTitle : nextEvent->title));

    if (nextEvent->fromHour >= 0) {
        const bool isRange =
            nextEvent->toHour >= 0 && nextEvent->toHour != nextEvent->fromHour;
        storeText(fresh.timeText, sizeof(fresh.timeText),
                  isRange ? formatEventTimeRange(fresh.startTime, fresh.endTime)
                          : formatEventTime(fresh.startTime));
    } else {
        storeText(fresh.timeText, sizeof(fresh.timeText), String("All day"));
    }

    {
        DataLock lock;
        data = fresh;
    }

    LOG_INFO("Next event found: " + String(fresh.title) + " on " +
             String(nextEvent->month) + "-" + String(nextEvent->day) +
             (nextEvent->fromHour >= 0 ? (" at " + String(fresh.timeText)) : " (all day)") +
             " (in " + String(minDaysAhead) + " days)");
}

CalendarSnapshot CalendarManager::snapshot() const {
    DataLock lock;
    return data;
}

// Function to read calendar events from birthdays.h
void CalendarManager::readCalendarEvents() {
    LOG_INFO_F("Reading calendar events from birthdays.h...");

    findNextEvent();
    markUpdated();

    const CalendarSnapshot s = snapshot();
    if (s.hasEvent) {
        LOG_INFO("Calendar event loaded: " + String(s.title));
    } else {
        LOG_INFO_F("No upcoming events found");
    }
}

// Check if event should be displayed now (once per 15 minutes)
bool CalendarManager::shouldDisplayNow() const {
    const unsigned long currentTime = millis();
    return (lastDisplayTime == 0 || (currentTime - lastDisplayTime >= DISPLAY_INTERVAL_MS));
}

// Check if current event is active (within time range)
bool CalendarManager::isEventActiveNow(const CalendarSnapshot& s) const {
    if (!s.hasEvent) {
        return false;
    }
    const time_t now = currentEpoch();
    return (now >= s.startTime && now <= s.endTime);
}

// Function to print next event on the screen
void CalendarManager::printNextEventToScreen() const {
    const CalendarSnapshot s = snapshot();

    if (!s.hasEvent || strlen(s.title) == 0) {
        drawString(String("No events"));
        lastDisplayTime = millis();
        return;
    }

    // Only show events on the day of the event.
    const time_t now = currentEpoch();
    struct tm timeinfoBuf;
    struct tm* timeinfo = gmtime_r(&now, &timeinfoBuf);
    struct tm eventTimeinfoBuf;
    struct tm* eventTimeinfo = gmtime_r(&s.startTime, &eventTimeinfoBuf);
    if (timeinfo == nullptr || eventTimeinfo == nullptr) {
        Clock::getInstance().skipCurrentDisplay();
        return;
    }

    const int currentMonth = timeinfo->tm_mon + 1;
    const int currentDay = timeinfo->tm_mday;
    const int eventMonth = eventTimeinfo->tm_mon + 1;
    const int eventDay = eventTimeinfo->tm_mday;

    if (eventMonth != currentMonth || eventDay != currentDay) {
        LOG_DEBUG("Event is not today (current: " + String(currentMonth) + "/" + String(currentDay) +
                  ", event: " + String(eventMonth) + "/" + String(eventDay) + "), skipping to next display");
        Clock::getInstance().skipCurrentDisplay();
        return;
    }

    const bool isActive = isEventActiveNow(s);
    LOG_DEBUG("Event is today, isActive=" + String(isActive ? "true" : "false"));

    // Timed events show while active; otherwise at most once per 15 minutes.
    if (!shouldDisplayNow() && !isActive) {
        Clock::getInstance().skipCurrentDisplay();
        return;
    }

    const String title = String(s.title);
    const String timeText = String(s.timeText);
    String tape;

    if (isActive || timeText.length() == 0 || timeText == "All day") {
        tape = truncateEventTitle(title, 20);
    } else {
        // Show the start time plus a shortened title.
        String shortTime = timeText;
        const int spacePos = shortTime.indexOf(' ');
        if (spacePos > 0) {
            shortTime = shortTime.substring(0, spacePos);
        }
        if (shortTime.length() > 5) {
            shortTime = shortTime.substring(0, 5); // Limit to HH:MM
        }
        tape = shortTime + " " + truncateEventTitle(title, 15);
    }

    LOG_INFO(">> Display: Calendar Event = " + tape + (isActive ? " (ACTIVE)" : ""));
    drawString(tape);

    lastDisplayTime = millis();
}

// Helper: Format event time as HH:MM
String CalendarManager::formatEventTime(time_t eventTime) const {
    struct tm timeinfoBuf;
    struct tm* timeinfo = gmtime_r(&eventTime, &timeinfoBuf);
    if (timeinfo == nullptr) {
        return String();
    }
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    return String(timeStr);
}

// Helper: Format event time range as "HH:MM-HH:MM"
String CalendarManager::formatEventTimeRange(time_t startTime, time_t endTime) const {
    struct tm startInfoBuf, endInfoBuf;
    struct tm* startInfo = gmtime_r(&startTime, &startInfoBuf);
    struct tm* endInfo = gmtime_r(&endTime, &endInfoBuf);
    if (startInfo == nullptr || endInfo == nullptr) {
        return String();
    }
    char timeStr[12];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d-%02d:%02d",
             startInfo->tm_hour, startInfo->tm_min,
             endInfo->tm_hour, endInfo->tm_min);
    return String(timeStr);
}

// Helper: Truncate event title to fit display
String CalendarManager::truncateEventTitle(const String& title, int maxLength) const {
    return TextUtils::truncate(title, maxLength);
}

// Check if calendar should be updated today
bool CalendarManager::shouldUpdateToday() const {
    if (lastUpdateDay == -1) {
        return true;
    }

    const time_t now = currentEpoch();
    if (now < Timing::MIN_VALID_EPOCH) {
        LOG_DEBUG_F("Time not synchronized, will update calendar when time is available");
        return true;
    }

    struct tm timeinfoBuf;
    struct tm* timeinfo = gmtime_r(&now, &timeinfoBuf);
    if (timeinfo == nullptr) {
        LOG_WARNING_F("Failed to get time info, will retry calendar update");
        return true;
    }

    return (lastUpdateDay != timeinfo->tm_mday);
}

// Mark calendar as updated for today
void CalendarManager::markUpdated() {
    const time_t now = currentEpoch();
    if (now < Timing::MIN_VALID_EPOCH) {
        LOG_WARNING_F("Time not synchronized, cannot mark calendar as updated");
        return;
    }

    struct tm timeinfoBuf;
    struct tm* timeinfo = gmtime_r(&now, &timeinfoBuf);
    if (timeinfo != nullptr) {
        lastUpdateDay = timeinfo->tm_mday;
        LOG_INFO("Calendar marked as updated for day " + String(lastUpdateDay));
    } else {
        LOG_WARNING_F("Failed to get time info, cannot mark calendar as updated");
    }
}
