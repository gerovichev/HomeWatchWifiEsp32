#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <Arduino.h>
#include "birthdays.h"

// Plain copy of the resolved "next event", handed out under dataMutex so the
// display core can format and draw without holding the lock.
struct CalendarSnapshot {
    char title[48] = {0};
    char timeText[16] = {0};   // "HH:MM", "HH:MM-HH:MM" or "All day"
    time_t startTime = 0;
    time_t endTime = 0;
    bool hasEvent = false;
};

class CalendarManager {
public:
    // Constructor
    CalendarManager();

    // Methods to interact with calendar data
    void readCalendarEvents();              // Reads events from birthdays.h
    void printNextEventToScreen() const;    // Prints next upcoming event
    bool shouldUpdateToday() const;         // Checks if calendar should be updated today
    void markUpdated();                     // Marks calendar as updated for today

private:
    CalendarSnapshot data;
    int lastUpdateDay;  // Last day when calendar was updated (1-31)
    mutable unsigned long lastDisplayTime;  // Last time event was displayed (millis)

    // Copies the resolved event under dataMutex.
    CalendarSnapshot snapshot() const;

    // Helper methods
    String formatEventTime(time_t eventTime) const;
    String formatEventTimeRange(time_t startTime, time_t endTime) const;
    String truncateEventTitle(const String& title, int maxLength = 20) const;
    void findNextEvent();                   // Find next upcoming event
    bool shouldDisplayNow() const;         // Check if event should be displayed now (once per 15 min)
    bool isEventActiveNow(const CalendarSnapshot& s) const; // Within the event's time range
    bool matchesHostname(const char* eventHostname) const; // Check if event matches current board hostname
};

#endif // CALENDAR_MANAGER_H
