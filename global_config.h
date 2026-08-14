#pragma once

#include <ArduinoOTA.h>
#include "Secret.h"
#include "led_display.h"
#include "logger.h"

// Configuration-related global variables
extern String lang_weather;
extern time_t sunrise;
extern time_t sunset;

extern String version_prg;
extern char grad;

extern float humidity_delta;
extern String hostname_m;
extern boolean isOTAreq;
extern boolean isMQTT;
extern String nameofWatch;
extern String macAddrSt;

extern String daysOfTheWeek[7];
extern boolean IS_DHT_CONNECTED;
extern bool isWebClientNeeded;
extern boolean isReadWeather;

void initPerDevice();
String getNumberWithZerro(int dig);
void drawString(const String& tape);
char getGradValue();
