#include "led_display.h"
#include "constants.h"
#include "logger.h"

// Global variables
bool newMessageAvailable = false;
MD_Parola M = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
//MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
String lastDisplayedText = "";

// LEDBuffer class removed to reduce code size

// Sets the intensity of the display
void setIntensity(byte intensity) { M.setIntensity(intensity); }

// Function to convert time_t to a readable string format
String formatTime(time_t rawTime) {
  int hours = (rawTime % 86400L) / 3600;
  int minutes = (rawTime % 3600) / 60;
  int seconds = rawTime % 60;

  char timeStr[Buffer::TIME_STRING_SIZE];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);

  return String(timeStr);
}

// Sets the intensity based on the current time
// Uses displayIntensity from config as base value
void setIntensityByTime(time_t timeNow) {
  // Use config intensity directly to respect user settings
  // The intensity from config is stored in global variable displayIntensity
  M.setIntensity(displayIntensity);
}

// Converts UTF-8 to Russian characters
String utf2rus(const String &source) {
  String target;
  target.reserve(source.length());

  for (int i = 0, k = source.length(); i < k; ++i) {
    unsigned char n = source[i];
    if (n >= 0xC0) {
      switch (n) {
      case 0xD0: {
        n = source[++i];
        if (n == 0x81) {
          n = 0xA8;
        } else if (n >= 0x90 && n <= 0xBF) {
          n += 0x30;
        }
        break;
      }
      case 0xD1: {
        n = source[++i];
        if (n == 0x91) {
          n = 0xB8;
        } else if (n >= 0x80 && n <= 0x8F) {
          n += 0x70;
        }
        break;
      }
      }
    }
    target += char(n);
  }

  return target;
}

// Draws a string on the LED display
void drawStringMax(const String &tape) {
  String convertedText = utf2rus(tape);
  // Always update if text is different, or if lastDisplayedText is empty (forced update)
  if (convertedText != lastDisplayedText || lastDisplayedText.length() == 0) {
    lastDisplayedText = convertedText;
    newMessageAvailable = true;
    // LED color is changed in individual display functions, not here
  }
}

// Displays the text on the LED display
void realDisplayText() {
  // Always call displayAnimate() to keep animation running
  // This must be called continuously for animations to work
  bool animationDone = M.displayAnimate();
  
  // If animation is done and we have a new message, update display
  if (animationDone && newMessageAvailable) {
    newMessageAvailable = false;
    M.displayReset();
    // LOG_INFO(">> Display: " + lastDisplayedText); // Disabled to reduce code size
    M.displayClear();

    if (lastDisplayedText.length() > 5) {
      M.displayText(lastDisplayedText.c_str(), PA_LEFT,
                    Display::SCROLL_SPEED_MS, Display::PAUSE_TIME_MS,
                    PA_SCROLL_LEFT, PA_NO_EFFECT);
    } else {
      M.displayText(lastDisplayedText.c_str(), PA_CENTER,
                    Display::SCROLL_SPEED_MS, Display::PAUSE_TIME_MS, PA_PRINT,
                    PA_NO_EFFECT);
    }
  }
}

// Force display text (for use in setup when display may not be ready)
void forceDisplayText() {
  if (newMessageAvailable) {
    newMessageAvailable = false;
    M.displayReset();
    // LOG_INFO(">> Force Display: " + lastDisplayedText); // Disabled to reduce code size
    M.displayClear();

    if (lastDisplayedText.length() > 5) {
      M.displayText(lastDisplayedText.c_str(), PA_LEFT,
                    Display::SCROLL_SPEED_MS, Display::PAUSE_TIME_MS,
                    PA_SCROLL_LEFT, PA_NO_EFFECT);
    } else {
      M.displayText(lastDisplayedText.c_str(), PA_CENTER,
                    Display::SCROLL_SPEED_MS, Display::PAUSE_TIME_MS, PA_PRINT,
                    PA_NO_EFFECT);
    }
  }
}

// Wait for animation to complete
void waitForAnimation() {
  while (!displayAnimate()) {
    delay(50);
  }
}

// Display text in setup (draw, force display, and wait for animation)
void displayTextInSetup(const String &text) {
  LOG_INFO(">> Display (Setup): " + text);
  // changeLEDColorForDisplay(DISPLAY_SETUP);  // Disabled - board LED not used
  drawStringMax(text);
  forceDisplayText();
  waitForAnimation();
}

// Animates the display
bool displayAnimate() { return M.displayAnimate(); }

// Initializes the LED matrix display
void matrixSetup() {
  // Initialize SPI explicitly for ESP32-C3
  // Note: MD_Parola should handle SPI, but explicit init helps with some modules
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // CS high = deselected
  
  // Initialize the display
  //SPI.begin();
  M.begin();
  
  // Clear display first to avoid all LEDs on
  M.displayClear();
  delay(50);
  
  // Configure display settings
  M.displaySuspend(false);
  M.setInvert(false);
  M.setFont(CRMrusTxt);
  
  // Set intensity - start with low value to avoid all LEDs on
  M.setIntensity(0);  // Start with minimum intensity
  delay(50);
  
  // Clear again after configuration
  M.displayClear();
  delay(50);
  
  // Now set the proper intensity
  M.setIntensity(Display::INTENSITY_NIGHT);
  
  // Reset and clear one more time
  M.displayReset();
  M.displayClear();
  delay(100);

  // Test display with a simple character to verify it works
  // If all LEDs are on, try changing HARDWARE_TYPE in led_display.h
  // Common types: FC16_HW, GENERIC_HW, PAROLA_HW
  
  // Process display animation to ensure display is ready
  for (int i = 0; i < 5; i++) {
    M.displayAnimate();
    delay(50);
  }
  
  LOG_INFO_F("LED matrix initialized");
}

// Prints the given text on the LED display
void printText(String text) {
  LOG_INFO(">> Display (Print): " + text);
  char dataText[Buffer::LED_BUFFER_SIZE];
  utf2rus("     " + text).toCharArray(dataText, Buffer::LED_BUFFER_SIZE);

  int textLength = strlen(dataText) - 6;

  if (textLength > 5) {
    for (int i = 0; i < textLength; i++) {
      M.print(&dataText[i]);
      // Use yield() instead of delay() to allow other tasks to run
      // Small delay is acceptable here for display animation
      yield();
      delay(250); // Keep delay for display timing, but allow yield
    }
  } else {
    M.print(&dataText[5]);
  }
}

