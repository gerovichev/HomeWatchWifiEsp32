#include "OTAUpdate.h"
#include "constants.h"
#include "logger.h"
#include "secure_client.h"

// Global variable definition
String pathOta;

// OTA callbacks
void update_started() {
  LOG_INFO_F("OTA update process started");
  printText(F("Update"));
}

void update_finished() {
  LOG_INFO_F("OTA update process finished");
  printText(F("Restart"));
}

void update_progress(int cur, int total) {
  int percent = 0;
  if (total > 0) {
    percent = (cur * 100) / total;
  }
  printText(String(percent, DEC) + " %");
  if (total > 0) {
    LOG_VERBOSE("OTA progress: " + String(cur) + "/" + String(total) +
                " bytes (" + String(percent) + "%)");
  } else {
    LOG_VERBOSE("OTA progress: " + String(cur) + "/unknown bytes (" +
                String(percent) + "%)");
  }
}

void update_error(int err) {
  LOG_ERROR("OTA update fatal error code: " + String(err) + " (" +
            httpUpdate.getLastErrorString() + ")");
}

// OTA initialization
void web_ota_init() {
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  // Note: ESP32 HTTPUpdate doesn't have setClientTimeout
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // Constructing OTA URL - optimize to reduce String allocations
  pathOta.reserve(strlen(webOTA_updateURL) + macAddrSt.length() +
                  hostname_m.length() + ip.length() + version_prg.length() +
                  50);
  pathOta = String(webOTA_updateURL) + F("?MAC=") + macAddrSt + F("&hst=") +
            hostname_m + F("&ip=") + ip + F("&ver=") + version_prg;
}

// Perform OTA update
void update_ota() {
  WiFiClientSecure client;
  setupSecureClient(client, "OTA server");
  client.setTimeout(Timing::OTA_CLIENT_TIMEOUT_MS);

  LOG_DEBUG("OTA URL: " + pathOta);
  LOG_DEBUG("OTA client timeout: " + String(Timing::OTA_CLIENT_TIMEOUT_MS) +
            " ms");

  // Perform the update and check the result
  t_httpUpdate_return ret = httpUpdate.update(client, pathOta, version_prg);

  LOG_DEBUG("OTA returned code: " + String(ret));

  // Handle update result
  switch (ret) {
  case HTTP_UPDATE_FAILED:
    LOG_ERROR("OTA update failed: Error " +
              String(httpUpdate.getLastError()) + " - " +
              httpUpdate.getLastErrorString());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    LOG_INFO_F("No OTA updates available");
    break;

  case HTTP_UPDATE_OK:
    LOG_INFO_F("OTA update completed successfully");
    break;
  }
}
