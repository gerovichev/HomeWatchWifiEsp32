#include "clock.h"
#include "constants.h"
#include "global_config.h"
#include "logger.h"
#include "led_display.h"
#include <TimeLib.h>

// External display object
extern MD_Parola M;

// Singleton instance of the Clock class
Clock &Clock::getInstance() {
  static Clock instance;
  return instance;
}

// Initialize the clock
void Clock::init() {
  LOG_INFO("Clock::init() called");
  buildDisplaySequence();   // Build display sequence
  LOG_INFO("Clock initialized: sequence length=" + String(displaySequenceLength) + 
           ", initial index=0, lastChangeTime=" + String(lastChangeTime));
  
  if (displaySequenceLength == 0) {
    LOG_ERROR("ERROR: Display sequence is empty! buildDisplaySequence() failed!");
  }
  
  currentDisplayIndex = 0;  // Reset index
  lastDisplayedMinute = -1; // Initialize minute tracking
  lastChangeTime = millis();
  isTransitioning = false;
}

// Detach - no longer needed with millis approach but kept for interface
// compatibility
void Clock::detach() {
  // No timer to detach
}

// Main loop for handling the clock updates
void Clock::loop() {
  unsigned long currentTime = millis();

  // Check if enough time has passed since last change
  // Add small buffer (100ms) to prevent rapid re-triggering
  unsigned long timeSinceLastChange = currentTime - lastChangeTime;
  bool timeExpired = timeSinceLastChange >= ((Timing::CLOCK_INTERVAL_SEC * 1000UL) - 100);

  // Check if animation is finished (returns true if done)
  bool animationDone = displayAnimate();
  
  // Log state periodically for debugging
  static unsigned long lastDebugLog = 0;
  if (currentTime - lastDebugLog > 10000) { // Every 10 seconds
    lastDebugLog = currentTime;
    LOG_INFO("Clock state: index=" + String(currentDisplayIndex) + 
             ", timeSinceLastChange=" + String(timeSinceLastChange) + 
             "ms, timeExpired=" + String(timeExpired ? "true" : "false") + 
             ", animationDone=" + String(animationDone ? "true" : "false"));
  }

  // We only proceed if BOTH time has expired AND animation is done
  if (!timeExpired) {
    return;  // Time hasn't expired yet, nothing to do
  }
  
  if (!animationDone) {
    return;  // Animation still in progress, wait
  }

  // Both conditions met: time expired AND animation done
  int previousIndex = currentDisplayIndex;
  LOG_INFO("Clock loop: conditions met, moving from index " + String(previousIndex) + " to next");
  
  // CRITICAL: Update timer FIRST to prevent immediate re-trigger in same loop iteration
  lastChangeTime = currentTime;
  
  // Move to next element or start from beginning
  currentDisplayIndex++;
  
  if (currentDisplayIndex >= displaySequenceLength) {
    currentDisplayIndex = 0;
    // Only log if we actually moved from last element to first
    if (previousIndex == displaySequenceLength - 1) {
      LOG_INFO_F("Display cycle completed, restarting");
    }
    // Clear last displayed text to force update of first element
    lastDisplayedText = "";
  }

  LOG_INFO("Clock loop: index changed from " + String(previousIndex) + " to " + String(currentDisplayIndex) + 
           " (sequence length=" + String(displaySequenceLength) + ")");

  // Execute current action from sequence
  if (displaySequenceLength > 0 && currentDisplayIndex < displaySequenceLength) {
    // Log which element we're displaying (for debugging) - use INFO level so it's visible
    String elementName = "Unknown";
    if (displaySequence[currentDisplayIndex] == &Clock::displayTime) elementName = "Time";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayDate) elementName = "Date";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayDay) elementName = "Day";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayWeather) elementName = "Weather";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayMaxTemp) elementName = "Feels Like";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayPressure) elementName = "Pressure";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayWeatherHumidity) elementName = "Weather Humidity";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayWeatherDescription) elementName = "Weather Description";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayUSD) elementName = "USD";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayEUR) elementName = "EUR";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayBTC) elementName = "BTC";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayHomeTemp) elementName = "Home Temp";
    else if (displaySequence[currentDisplayIndex] == &Clock::displayHomeHumidity) elementName = "Home Humidity";
    
    LOG_INFO(">> Displaying element [" + String(currentDisplayIndex) + "/" + String(displaySequenceLength - 1) + "]: " + elementName);
    
    // Execute the action (this sets newMessageAvailable via drawStringMax)
    executeDisplayAction();
    
    // After execution, newMessageAvailable is set and realDisplayText() will handle it
  } else {
    LOG_ERROR("Clock loop: Invalid index " + String(currentDisplayIndex) + " or sequence length " + String(displaySequenceLength));
  }
}

// Executes current action from sequence
void Clock::executeDisplayAction() {
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
void Clock::displayMaxTemp() { weatherManager.printMaxTempToScreen(); }
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

// Builds display sequence based on available data
void Clock::buildDisplaySequence() {
  LOG_INFO("buildDisplaySequence() called");
  int index = 0;

  // Always show time and date
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayDate;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayDay;

  // Weather
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayWeather;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayMaxTemp;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayPressure;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayWeatherHumidity;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayWeatherDescription;

  // Currency
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayUSD;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayEUR;
  displaySequence[index++] = &Clock::displayTime;
  displaySequence[index++] = &Clock::displayBTC;

  // Home sensors (only if connected)
  if (IS_DHT_CONNECTED) {
    displaySequence[index++] = &Clock::displayTime;
    displaySequence[index++] = &Clock::displayHomeTemp;
    displaySequence[index++] = &Clock::displayTime;
    displaySequence[index++] = &Clock::displayHomeHumidity;
  }

  displaySequenceLength = index;
  LOG_INFO("Display sequence built with " + String(displaySequenceLength) + " items");
  
  if (displaySequenceLength == 0) {
    LOG_ERROR("ERROR: buildDisplaySequence() resulted in empty sequence!");
  }
}

// Checks for minute change and switches to time display
void Clock::checkMinuteChange() {
  // Get current minute using TimeLib (more reliable)
  int currentMinute = minute();

  // If minute changed
  if (lastDisplayedMinute != -1 && currentMinute != lastDisplayedMinute) {
    // Check if we're currently showing time
    bool isCurrentlyShowingTime =
        (displaySequence[currentDisplayIndex] == &Clock::displayTime);

    if (isCurrentlyShowingTime) {
      // If already showing time, just update the display text
      // BUT DON'T reset lastChangeTime - let the normal cycle continue!
      executeDisplayAction();
      LOG_INFO("Minute changed to " + String(currentMinute) + ", updated time display (timer not reset)");
    } else {
      // Not showing time - don't interrupt, let normal sequence continue
      LOG_INFO("Minute changed to " + String(currentMinute) + ", continuing normal sequence");
    }
  }

  // Update last displayed minute
  lastDisplayedMinute = currentMinute;
}

// Finds next time index in display sequence
int Clock::findNextTimeIndex() {
  // Search for time starting from current index
  for (int i = currentDisplayIndex; i < displaySequenceLength; i++) {
    if (displaySequence[i] == &Clock::displayTime) {
      return i;
    }
  }

  // If not found, search from beginning of sequence
  for (int i = 0; i < currentDisplayIndex; i++) {
    if (displaySequence[i] == &Clock::displayTime) {
      return i;
    }
  }

  // If not found at all, return first element (which is always time)
  return 0;
}

Dht22_manager &Clock::getDht22() { return dht22_manager; }

WeatherManager &Clock::getWeatherManager() { return weatherManager; }

CurrencyManager &Clock::getCurrencyManager() { return currencyManager; }

// Initialize clock process
void init_clock_process() { Clock::getInstance().init(); }

// Detach the timer interrupt for the clock process
void detachInterrupt_clock_process() { Clock::getInstance().detach(); }

// Main clock loop
void clock_loop() { Clock::getInstance().loop(); }

