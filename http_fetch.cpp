#include "http_fetch.h"

#include "constants.h"
#include "error_handler.h"
#include "logger.h"
#include "secure_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {

// Turns a non-OK HTTP code into an explanatory log line. Kept separate so the
// attempt loop below stays readable.
void logHttpFailure(const char *tag, HTTPClient &http, int httpCode) {
  const char *reason;

  if (httpCode == -1) {
    reason = "connection not established - TLS handshake or DNS failure";
    LOG_DEBUGF("%s: WiFi %s, RSSI %d", tag,
               WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
               (int)WiFi.RSSI());
  } else if (httpCode < 0) {
    reason = "HTTP client error";
  } else if (httpCode >= 400 && httpCode < 500) {
    reason = "client error - check authentication/authorization";
    String body = http.getString();
    if (body.length() > 0) {
      LOG_DEBUGF("%s: error body: %.200s", tag, body.c_str());
    }
  } else if (httpCode >= 500) {
    reason = "server error";
  } else {
    reason = "unexpected status";
  }

  LOG_WARNINGF("%s: request failed with code %d (%s)", tag, httpCode, reason);
}

// One request/response round trip. Returns an empty String on any failure.
String performGet(const HttpFetchOptions &opts, HTTPClient &http) {
  const unsigned long startTime = millis();
  const int httpCode = http.GET();
  LOG_DEBUGF("%s: GET completed in %lums, code %d", opts.tag,
             millis() - startTime, httpCode);

  if (httpCode != HTTP_CODE_OK) {
    logHttpFailure(opts.tag, http, httpCode);
    return String();
  }

  return http.getString();
}

} // namespace

bool httpFetchWithRetry(const HttpFetchOptions &opts,
                        const HttpPayloadHandler &handler) {
  if (opts.url == nullptr) {
    LOG_ERROR_F("httpFetchWithRetry called with null URL");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERRORF("%s: WiFi not connected, skipping fetch", opts.tag);
    return false;
  }

  int attempts = 0;

  while (attempts < opts.maxAttempts) {
    if (WiFi.status() != WL_CONNECTED) {
      LOG_ERRORF("%s: WiFi dropped mid-fetch, aborting", opts.tag);
      break;
    }

    LOG_DEBUGF("%s: attempt %d/%d", opts.tag, attempts + 1, opts.maxAttempts);

    // Scoped per attempt on purpose: mbedTLS state is not reusable after a
    // failed handshake, so retrying on the same client just fails again.
    {
      WiFiClientSecure client;
      setupSecureClient(client, opts.tag);
      HTTPClient http;
      http.setTimeout(opts.timeoutMs);

      if (http.begin(client, opts.url)) {
        if (opts.bearerToken != nullptr) {
          http.addHeader(F("Authorization"),
                         String("Bearer ") + String(opts.bearerToken));
          http.addHeader(F("Content-Type"), F("application/json"));
        }

        const String payload = performGet(opts, http);
        http.end();
        client.stop();
        yield();

        if (payload.length() > 0 && handler(payload)) {
          return true;
        }

        LOG_WARNINGF("%s: %s", opts.tag,
                     payload.length() == 0 ? "empty payload"
                                           : "payload rejected by parser");
      } else {
        LOG_ERRORF("%s: failed to begin HTTP connection", opts.tag);
        client.stop();
      }
    }

    yield();
    attempts++;

    if (attempts < opts.maxAttempts) {
      LOG_WARNINGF("%s: retrying (%d/%d)...", opts.tag, attempts, opts.maxAttempts);
      delay(Timing::RETRY_DELAY_MS);
      yield();
    }
  }

  ErrorHandler::handleError(ErrorHandler::ERROR_API,
                            String(opts.tag) + " fetch failed after " +
                                String(opts.maxAttempts) +
                                " attempts, keeping cached value",
                            attempts, opts.maxAttempts);
  return false;
}
