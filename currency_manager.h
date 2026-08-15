#pragma once

#include "global_config.h"

class CurrencyManager {
public:
  CurrencyManager();

  void initialize();
  void displayUSDToScreen();
  void displayEURToScreen();
  void displayBTCToScreen();

private:
  // Using const char* directly to avoid String copies in RAM
  const char *bearerTokenCurrency;
  const char *bearerTokenCrypto;
  const char *pathCurrencyUSD;
  const char *pathCurrencyEUR;
  const char *pathCryptoBTC;

  float dataUSDValue;
  float dataEURValue;
  float dataBTCValue;

  void fetchInto(const char *path, const char *token,
                 float (*parseFn)(const String &), const char *label,
                 float &target);

  // Reads one rate under dataMutex; the values are written from the other core.
  float valueOf(const float &field) const;
};
