#include "clock.h"
#include "constants.h"
#include "device_state.h"
#include "global_config.h"
#include "logger.h"
#include <TimeLib.h>

// Singleton instance of the Clock class
Clock &Clock::getInstance() {
  static Clock instance;
  return instance;
}

// Initialize the clock
void Clock::init() {
  buildDisplaySequence();   // Build display sequence
  currentDisplayIndex = 0;  // Reset index
  showingTime = true;       // Rotation always opens on the clock
  lastDisplayedMinute = -1; // Initialize minute tracking
  lastChangeTime = millis();
}

// Main loop for handling the clock updates
void Clock::loop() {
  unsigned long currentTime = millis();

  // Check if enough time has passed since last change
  bool timeExpired =
      (currentTime - lastChangeTime) >= (Timing::CLOCK_INTERVAL_SEC * 1000UL);

  // Check if animation is finished (returns true if done)
  // We only proceed if BOTH time has expired AND animation is done
  if (timeExpired && displayAnimate()) {
    // Alternate clock / data screen; advance to the next data screen only
    // after its clock slot has been shown.
    if (showingTime) {
      showingTime = false;
    } else {
      showingTime = true;
      currentDisplayIndex++;
      if (currentDisplayIndex >= displaySequenceLength) {
        currentDisplayIndex = 0;
        LOG_VERBOSE_F("Display cycle completed, restarting");
      }
    }

    lastChangeTime = currentTime; // Reset timer before execution
    executeDisplayAction();       // Action might override lastChangeTime via skipCurrentDisplay
  }
}

// Executes current action from sequence.
//
// Deliberately takes no lock: each display action reads a snapshot of the data
// it needs under dataMutex internally and formats/draws outside the critical
// section. Locking here instead would hold the mutex across the whole render -
// including String building and, before DHT reads moved to the data task, a
// blocking sensor read - and stall the network core on the other side.
void Clock::executeDisplayAction() {
  if (showingTime) {
    displayTime();
    return;
  }

  if (currentDisplayIndex < displaySequenceLength &&
      displaySequence[currentDisplayIndex] != nullptr) {
    (this->*displaySequence[currentDisplayIndex])();
  }
}

// Wrapper methods for display (used as function pointers)
void Clock::displayTime() { printTimeToScreen(); }
void Clock::displayDate() { printDateToScreen(); }
void Clock::displayDay() { printDayToScreen(); }
void Clock::displayWeather() { weatherManager.printWeatherToScreen(); }
void Clock::displayFeelsLike() { weatherManager.printFeelsLikeToScreen(); }
void Clock::displayPressure() { weatherManager.printPressureToScreen(); }
void Clock::displayWeatherHumidity() { weatherManager.printHumidityToScreen(); }
void Clock::displayWeatherDescription() {
  weatherManager.printDescriptionWeatherToScreen();
}
void Clock::displayUSD() { currencyManager.displayUSDToScreen(); }
void Clock::displayEUR() { currencyManager.displayEURToScreen(); }
void Clock::displayBTC() { currencyManager.displayBTCToScreen(); }
void Clock::displayHomeTemp() { dht22_manager.printHomeTemp(); }
void Clock::displayHomeHumidity() { dht22_manager.printHumidity(); }
void Clock::displayCalendar() { calendarManager.printNextEventToScreen(); }

// Appends one data screen, dropping it (loudly) rather than running off the
// end of the fixed-size array.
void Clock::addScreen(DisplayAction action) {
  if (displaySequenceLength >= Display::MAX_SEQUENCE_LENGTH) {
    LOG_ERROR_F("Display sequence full, screen dropped - raise Display::MAX_SEQUENCE_LENGTH");
    return;
  }
  displaySequence[displaySequenceLength++] = action;
}

// Builds display sequence based on available data
void Clock::buildDisplaySequence() {
  displaySequenceLength = 0;

  addScreen(&Clock::displayDate);
  addScreen(&Clock::displayDay);

  // Weather - skipped entirely on devices configured without it, so they do
  // not rotate through five screens of never-populated data.
  if (DeviceState::getInstance().isReadWeather()) {
    addScreen(&Clock::displayWeather);
    addScreen(&Clock::displayFeelsLike);
    addScreen(&Clock::displayPressure);
    addScreen(&Clock::displayWeatherHumidity);
    addScreen(&Clock::displayWeatherDescription);
  }

  addScreen(&Clock::displayUSD);
  addScreen(&Clock::displayEUR);
  addScreen(&Clock::displayBTC);

  // Home sensors (only if connected)
  if (DeviceState::getInstance().isDhtConnected()) {
    addScreen(&Clock::displayHomeTemp);
    addScreen(&Clock::displayHomeHumidity);
  }

  addScreen(&Clock::displayCalendar);

  LOG_DEBUG("Display sequence built with " + String(displaySequenceLength) +
            " data screens (clock shown between each)");
}

// Checks for minute change and switches to time display
void Clock::checkMinuteChange() {
  // Get current minute using TimeLib (more reliable)
  int currentMinute = minute();

  if (lastDisplayedMinute != -1 && currentMinute != lastDisplayedMinute) {
    if (!showingTime) {
      // Jump to the clock slot for the current data screen. The rotation
      // resumes from the same data screen afterwards.
      showingTime = true;
    }
    executeDisplayAction();     // Immediately show time
    lastChangeTime = millis();  // Stay on it for the full interval
    LOG_VERBOSE_F("Minute changed, switched to time display");
  }

  lastDisplayedMinute = currentMinute;
}

// Skip current display item and force immediate transition
void Clock::skipCurrentDisplay() {
  // Force timeExpired on the next loop check. No index juggling needed: loop()
  // owns the time/data alternation, so it advances correctly on its own.
  lastChangeTime = millis() - (Timing::CLOCK_INTERVAL_SEC * 1000UL);
}

Dht22_manager &Clock::getDht22() { return dht22_manager; }

WeatherManager &Clock::getWeatherManager() { return weatherManager; }

CurrencyManager &Clock::getCurrencyManager() { return currencyManager; }

CalendarManager &Clock::getCalendarManager() { return calendarManager; }

// Initialize clock process
void init_clock_process() { Clock::getInstance().init(); }

// Main clock loop
void clock_loop() { Clock::getInstance().loop(); }
