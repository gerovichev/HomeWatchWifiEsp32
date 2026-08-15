#pragma once

#include "TimeManager.h"
#include "currency_manager.h"
#include "dht22_manager.h"
#include "Secret.h"
#include "weather_manager.h"
#include "calendar_manager.h"
#include "constants.h"

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

  typedef void (Clock::*DisplayAction)();

  void executeDisplayAction();
  void buildDisplaySequence();
  void addScreen(DisplayAction action);

  // Wrapper methods for display
  void displayTime();
  void displayDate();
  void displayDay();
  void displayWeather();
  void displayFeelsLike();
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
  bool showingTime = true;          // Rotation alternates time / data screen

  WeatherManager weatherManager;
  CurrencyManager currencyManager;
  CalendarManager calendarManager;

  // Only the data screens are stored. The rotation shows the clock between
  // every one of them, so interleaving displayTime into this table (which is
  // what it used to do) would double its size and force findNextTimeIndex()
  // to search for slots that are really just "every other entry".
  DisplayAction displaySequence[Display::MAX_SEQUENCE_LENGTH];
  int displaySequenceLength = 0;
};

// Initialize clock process functions
void init_clock_process();
void clock_loop();
