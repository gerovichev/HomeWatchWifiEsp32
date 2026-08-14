#pragma once

#include <WiFiClientSecure.h>
#include "logger.h"
#include "root_certs.h"

/**
 * Setup secure SSL client with certificate validation.
 * Requires the system clock to already be synced (see setClock() in
 * location_manager.cpp) - mbedTLS rejects certificates as "not yet valid"
 * if the ESP32's clock is still at its power-on epoch.
 */
inline void setupSecureClient(WiFiClientSecure& client, const char* domain = nullptr) {
    client.setCACert(ROOT_CA_BUNDLE);

    if (domain) {
        LOG_DEBUG("Secure client configured with CA validation for: " + String(domain));
    } else {
        LOG_DEBUG_F("Secure client configured with CA validation");
    }
}
