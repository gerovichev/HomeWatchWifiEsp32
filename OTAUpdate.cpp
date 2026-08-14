#include "OTAUpdate.h"
#include "constants.h"
#include "logger.h"
#include "secure_client.h"

// Global variable definition
String pathOta;

// OTA callbacks.
//
// These run on the data-update task (core 0) while loop() on core 1 is driving
// MD_Parola over the same SPI bus and the same MD_Parola object. Writing to the
// display from here would interleave SPI transactions with the display core, so
// these callbacks only log - the panel keeps showing the normal rotation until
// the device reboots into the new firmware.
void update_started() {
  LOG_INFO_F("OTA update process started");
}

void update_finished() {
  LOG_INFO_F("OTA update process finished, restarting");
}

void update_progress(int cur, int total) {
  if (total > 0) {
    // Log on decade boundaries only: the callback fires per chunk and OTA is
    // already the slowest thing the device does.
    static int lastLoggedDecile = -1;
    const int percent = (cur * 100) / total;
    const int decile = percent / 10;
    if (decile != lastLoggedDecile) {
      lastLoggedDecile = decile;
      LOG_INFO("OTA progress: " + String(cur) + "/" + String(total) +
               " bytes (" + String(percent) + "%)");
    }
  } else {
    LOG_VERBOSE("OTA progress: " + String(cur) + "/unknown bytes");
  }
}

void update_error(int err) {
  LOG_ERROR("OTA update fatal error code: " + String(err) + " (" +
            httpUpdate.getLastErrorString() + ")");
}

// Builds the OTA query string. Called per update check rather than once at
// init because `ip` is refreshed by location_init() on every data cycle, and a
// URL built at boot would keep reporting a stale address forever.
static void buildOtaPath() {
  pathOta = "";
  pathOta.reserve(strlen(webOTA_updateURL) + macAddrSt.length() +
                  hostname_m.length() + ip.length() + version_prg.length() +
                  50);
  pathOta = String(webOTA_updateURL) + F("?MAC=") + macAddrSt + F("&hst=") +
            hostname_m + F("&ip=") + ip + F("&ver=") + version_prg;
}

// OTA initialization
void web_ota_init() {
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  // Note: ESP32 HTTPUpdate doesn't have setClientTimeout
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  buildOtaPath();
}

// Perform OTA update
void update_ota() {
  buildOtaPath();

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
