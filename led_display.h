#pragma once

#include <MD_Parola.h>
#include <vector>
#include "fonts.h"
#include "global_config.h"

// Define the number of devices in the chain and the hardware interface
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// Pin configuration for different ESP32 boards
// Detect ESP32-C3 (GOOUUU-ESP32-C3) vs standard ESP32
#if defined(CONFIG_FREERTOS_UNICORE) || defined(ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3 (GOOUUU-ESP32-C3) configuration
  // According to https://docs.cirkitdesigner.com/component/2c3dcfa5-a9b9-4f6a-8905-3a8f902bc0a6/goouuu-esp32-c3
  #define CLK_PIN 7   // Clock pin (GPIO7) - SPI CLK
  #define DATA_PIN 5  // Data pin (GPIO5) - SPI MOSI
  #define CS_PIN 6    // Chip select pin (GPIO6) - SPI CS
  #define USE_EXPLICIT_SPI_PINS 1  // ESP32-C3 requires explicit SPI pins in constructor
#else
  // Standard ESP32 configuration
  #define CLK_PIN 18  // Clock pin (GPIO18) - SPI CLK
  #define DATA_PIN 23 // Data pin (GPIO23) - SPI MOSI
  #define CS_PIN 5    // Chip select pin (GPIO5) - SPI CS
  #define USE_EXPLICIT_SPI_PINS 0  // Standard ESP32 uses hardware SPI (only CS needed)
#endif

// LED_MAX_BUF moved to constants.h as Buffer::LED_BUFFER_SIZE

extern bool newMessageAvailable;



// Function declarations
void setIntensity(byte intensity);
String formatTime(time_t rawTime);
void setIntensityByTime(time_t timeNow);
String utf2rus(const String& source);
void drawStringMax(const String& tape);
void realDisplayText();
void forceDisplayText();
void waitForAnimation();
void displayTextInSetup(const String& text);
bool displayAnimate();
void matrixSetup();
void printText(String text);
