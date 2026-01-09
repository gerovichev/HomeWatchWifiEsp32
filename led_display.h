#pragma once

#include <MD_Parola.h>
#include "fonts.h"
#include "global_config.h"

// Define the number of devices in the chain and the hardware interface
// If all LEDs are on, try changing to GENERIC_HW or other types
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW // Changed from FC16_HW - more compatible
#define MAX_DEVICES 4  // Change to 1, 2, or 4 depending on your setup

// ESP32-C3 pin configuration for GOOUUU-ESP32-C3
// SPI pins for MAX7219 LED matrix
#define CLK_PIN 6   // Clock pin (GPIO6) - SPI CLK
#define DATA_PIN 7   // Data pin (GPIO7) - SPI MOSI
#define CS_PIN 5     // Chip select pin (GPIO5) - SPI CS

// LED_MAX_BUF moved to constants.h as Buffer::LED_BUFFER_SIZE

extern bool newMessageAvailable;
extern String lastDisplayedText;

// LEDBuffer class removed to reduce code size

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

