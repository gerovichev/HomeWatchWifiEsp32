#pragma once

#include <Arduino.h>
#include <functional>

/**
 * Shared HTTPS fetch-with-retry used by every outbound API call.
 *
 * Before this existed, weather / timezone / external-IP / currency each carried
 * their own copy of the same attempt loop, and every change to TLS handling,
 * timeouts or retry policy had to be made four times.
 *
 * The caller supplies only the parts that actually differ: the URL, an optional
 * bearer token, and a callback that turns a payload into stored state. The
 * callback returns false for a well-formed response that still isn't usable
 * (bad JSON, wrong status field, empty body), which counts as a failed attempt
 * and triggers a retry.
 */
struct HttpFetchOptions {
  const char *url = nullptr;
  const char *tag = "API";  // Host label used in log lines
  const char *bearerToken = nullptr;  // Adds Authorization/Content-Type when set
  int timeoutMs = 1500;
  int maxAttempts = 3;
};

using HttpPayloadHandler = std::function<bool(const String &payload)>;

/**
 * Runs the request, retrying up to opts.maxAttempts. Returns true if handler
 * accepted a payload. On terminal failure the outcome is routed through
 * ErrorHandler and the caller's previously cached data is left untouched.
 */
bool httpFetchWithRetry(const HttpFetchOptions &opts,
                        const HttpPayloadHandler &handler);
