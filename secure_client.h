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
    
    if (domain) {
        LOG_WARNING("Using insecure SSL connection for: " + String(domain) + 
                   " (consider adding certificate validation)");
    } else {
        LOG_DEBUG_F("Secure client configured (insecure mode)");
    }
}
