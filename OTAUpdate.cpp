#include "OTAUpdate.h"
#include "constants.h"
#include "logger.h"
#include "secure_client.h"
#include "location_manager.h"

// Global variable definition
String pathOta;

// OTA callbacks
void update_started() {
  LOG_INFO_F("OTA update process started");
  printText("Update");
}

void update_finished() {
  LOG_INFO_F("OTA update process finished");
  printText("Restart");
}

void update_progress(int cur, int total) {
  int percent = cur / (total / 100);
  printText(String(percent, DEC) + " %");
  // LOG_VERBOSE("OTA progress: " + String(cur) + "/" + String(total) +
  //             " bytes (" + String(percent) + "%)"); // Disabled to save code size
}

void update_error(int err) {
  // Optimized logging - avoid String concatenation
  if (Serial) {
    Serial.print(F("[ERROR]  "));
    Serial.print(F(" "));
    Serial.print(F("OTA update fatal error code: "));
    Serial.print(err);
    Serial.print(F(" ("));
    Serial.print(httpUpdate.getLastErrorString());
    Serial.println(F(")"));
  }
}

// OTA initialization
void web_ota_init() {
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  // Note: ESP32 HTTPUpdate doesn't have setTimeout method
  // Timeout is handled by the WiFiClientSecure client

  // Constructing OTA URL - optimize to reduce String allocations
  pathOta.reserve(strlen(webOTA_updateURL) + macAddrSt.length() +
                  hostname_m.length() + ip.length() + version_prg.length() +
                  50);
  pathOta = String(webOTA_updateURL) + String("?MAC=") + macAddrSt + String("&hst=") +
            hostname_m + String("&ip=") + ip + String("&ver=") + version_prg;
}

// Perform OTA update
void update_ota() {
  WiFiClientSecure client;
  setupSecureClient(client, "OTA server");
  client.setTimeout(Timing::OTA_CLIENT_TIMEOUT_MS);

  // LOG_DEBUG("OTA URL: " + pathOta); // Disabled to save code size

  // Perform the update and check the result
  t_httpUpdate_return ret = httpUpdate.update(client, pathOta, version_prg);

  // LOG_DEBUG("OTA returned code: " + String(ret)); // Disabled to save code size

  // Handle update result
  switch (ret) {
  case HTTP_UPDATE_FAILED:
    // Optimized logging - avoid String concatenation
    if (Serial) {
      Serial.print(F("[ERROR]  "));
      Serial.print(F(" "));
      Serial.print(F("OTA update failed: Error "));
      Serial.print(httpUpdate.getLastError());
      Serial.print(F(" - "));
      Serial.println(httpUpdate.getLastErrorString());
    }
    break;

  case HTTP_UPDATE_NO_UPDATES:
    LOG_INFO_F("No OTA updates available");
    break;

  case HTTP_UPDATE_OK:
    LOG_INFO_F("OTA update completed successfully");
    break;
  }
}

