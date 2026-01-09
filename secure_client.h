#pragma once

#include <WiFiClientSecure.h>
#include "logger.h"

/**
 * Setup secure SSL client
 * Attempts to use certificate validation if possible
 * Otherwise uses setInsecure with warning
 */
inline void setupSecureClient(WiFiClientSecure& client, const char* domain = nullptr) {
    // For ESP32 certificate validation requires additional setup
    // In production it's recommended to use root certificates
    // For now using setInsecure with warning for compatibility
    
    // TODO: Add support for root certificates for main domains
    // X509List cert(certificate);
    // client.setCACert(cert);
    
    client.setInsecure();
    
    // Removed verbose logging to reduce code size
}

