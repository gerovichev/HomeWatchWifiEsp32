#pragma once

#include "global_config.h"

// Structure to store configuration
struct Config {
  float latitude = 0.0f;
  float longitude = 0.0f;
  String ip;
};

// External variables
extern String ip;
extern float latitude;
extern float longitude;
extern Config config;

// Function prototypes
void loadConfiguration();
void saveConfiguration();
void setClock();
// Returns true only if it actually refreshed `config` - callers must not
// persist the config when this reports failure.
bool getLocationAPI(const String &ip);
// Returns the external IP, or an empty String if it could not be determined.
String getIp();
void location_init();
