#pragma once

// Root CA certificates for TLS certificate validation of all outbound HTTPS
// calls made via setupSecureClient() (secure_client.h).
//
// ROOT_CA_BUNDLE is a single PEM buffer containing multiple concatenated
// root certificates. WiFiClientSecure::setCACert() / mbedtls_x509_crt_parse()
// parse each certificate block in sequence, so any one of these roots can
// anchor the chain presented by the server.
//
// Covers, as of 2026-08:
//   - ISRG Root X1                          -> api.timezonedb.com, *.duckdns.org (Let's Encrypt)
//   - USERTrust RSA Certification Authority -> api.openweathermap.org (Sectigo)
//   - GlobalSign Root CA                    -> api.ipify.org (Google Trust Services)
//
// If a service migrates to a different CA, TLS handshakes for that host will
// fail (visible as HTTP code -1 in the logs) until its new root is added here.
extern const char ROOT_CA_BUNDLE[];
