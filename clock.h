#pragma once

#include "TimeManager.h"
#include "currency_manager.h"
#include "dht22_manager.h"
#include "Secret.h"
#include "weather_manager.h"
#include "calendar_manager.h"
#include "constants.h"
#include <Ticker.h>

// TIMER_INTERVAL_MS and DISPLAY_CYCLE_LENGTH moved to constants.h

class Clock {
public:
  static Clock &getInstance(); // Singleton access to instance

  void init();
  void loop();
  void checkMinuteChange(); // Public method to check for minute change
  void skipCurrentDisplay(); // Skip current item in sequence
  Dht22_manager &getDht22();
  WeatherManager &getWeatherManager();
  CurrencyManager &getCurrencyManager();
  CalendarManager &getCalendarManager();

private:
  Clock() = default; // Private constructor for singleton pattern

  // Delete copy constructor and assignment operator
  Clock(const Clock &) = delete;
  Clock &operator=(const Clock &) = delete;

  Dht22_manager dht22_manager;

  void executeDisplayAction();
  void buildDisplaySequence();
  int findNextTimeIndex();

  // Array of function pointers for display
  typedef void (Clock::*DisplayAction)();

  // Appends one screen, refusing to write past the end of displaySequence.
  void addDisplayAction(DisplayAction action);
  // Appends the paired "time, then <screen>" the rotation is built from.
  void addTimedDisplayAction(DisplayAction action);

  // Wrapper methods for display
  void displayTime();
  void displayDate();
  void displayDay();
  void displayWeather();
  void displayMaxTemp();
  void displayPressure();
  void displayWeatherHumidity();
  void displayWeatherDescription();
  void displayUSD();
  void displayEUR();
  void displayBTC();
  void displayHomeTemp();
  void displayHomeHumidity();
  void displayCalendar();

  int currentDisplayIndex = 0;
  int lastDisplayedMinute = -1;     // Track last displayed minute
  unsigned long lastChangeTime = 0; // Track when last display change happened

  WeatherManager weatherManager;
  CurrencyManager currencyManager;
  CalendarManager calendarManager;

  DisplayAction displaySequence[Display::MAX_SEQUENCE_LENGTH];
  int displaySequenceLength = 0;
};

// Initialize clock process functions
void init_clock_process();
void clock_loop();
