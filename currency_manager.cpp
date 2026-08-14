#include "currency_manager.h"
#include "constants.h"
#include "logger.h"
#include "error_handler.h"
#include "secure_client.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>

extern SemaphoreHandle_t dataMutex;

// ─────────────────────────────────────────────────────────────────────────────
// File-local helper functions
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Execute http.GET(), handle all error cases and return the raw payload.
 * Returns an empty String on any error (already logged).
 */
static String performGetRequest(HTTPClient &http) {
    LOG_VERBOSE("Calling http.GET()...");
    unsigned long startTime = millis();
    int httpCode = http.GET();
    unsigned long elapsed = millis() - startTime;
    LOG_DEBUG("http.GET() completed in " + String(elapsed) + "ms, code: " + String(httpCode));

    if (httpCode != HTTP_CODE_OK) {
        String errorMsg = "HTTP request failed with code: " + String(httpCode, DEC);
        if (httpCode == -1) {
            errorMsg += " (Connection failed - SSL handshake likely failed)";
            LOG_DEBUG("Error -1 typically means: connection not established, SSL handshake failed, or DNS resolution failed");
            LOG_DEBUG("WiFi status check: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
        } else if (httpCode < 0) {
            errorMsg += " (HTTP client error)";
        } else if (httpCode >= 400 && httpCode < 500) {
            errorMsg += " (Client error - check authentication/authorization)";
            String errorBody = http.getString();
            if (errorBody.length() > 0) {
                LOG_DEBUG("Error response body: " + errorBody.substring(0, min(200, (int)errorBody.length())));
            }
        } else if (httpCode >= 500) {
            errorMsg += " (Server error)";
        }
        LOG_WARNING(errorMsg);
        return String();
    }

    String payload = http.getString();
    LOG_VERBOSE("Payload length: " + String(payload.length()));
    return payload;
}

/** Parse a currency (USD/EUR) JSON payload – expects {"state": <value>}. */
static float parseCurrencyJson(const String &payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        LOG_ERROR("deserializeJson() failed: " + String(error.c_str()));
        return 0.0f;
    }
    // as<float>() rather than `| 0.0f`: Home Assistant serialises state as a
    // JSON *string* ("2.96"), and the `|` operator falls back to the default
    // for anything that is not already a number.
    return doc[F("state")].as<float>();
}

/**
 * Parse a crypto JSON payload.
 * Supports the new Home Assistant sensor format {"state": <value>}
 * and legacy CoinCap v3 format {"timestamp":..., "data":["87214.89"]}.
 */
static float parseCryptoJson(const String &payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        LOG_ERROR("deserializeJson() failed: " + String(error.c_str()));
        return 0.0f;
    }
    // as<float>() throughout: both formats deliver the price as a JSON string,
    // which the `|` fallback operator would discard in favour of the default.
    if (!doc[F("state")].isNull()) {
        return doc[F("state")].as<float>();
    }
    if (doc[F("data")].is<JsonArray>()) {
        JsonArray arr = doc[F("data")].as<JsonArray>();
        if (arr.size() > 0) {
            return arr[0].as<float>();
        }
    }
    LOG_ERROR("Failed to parse crypto price from response");
    return 0.0f;
}

/**
 * Generic retry loop for fetching a float value over HTTPS.
 * @param path        Full URL to request.
 * @param bearerToken Bearer token for the Authorization header.
 * @param parseFn     Function that extracts a float from the raw payload.
 */
static float fetchWithRetry(const char *path, const char *bearerToken,
                            std::function<float(const String &)> parseFn) {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_ERROR_F("WiFi not connected, cannot fetch data");
        return 0.0f;
    }

    const int maxAttempts = Retry::MAX_ATTEMPTS_CURRENCY;
    int attempts = 0;
    bool success = false;
    float value = 0.0f;

    while (attempts < maxAttempts && !success) {
        if (WiFi.status() != WL_CONNECTED) {
            LOG_ERROR("WiFi disconnected during fetch (attempt " + String(attempts + 1) + ")");
            break;
        }

        yield();

        WiFiClientSecure client;
        setupSecureClient(client, "API");
        HTTPClient http;
        http.setTimeout(Timing::HTTP_TIMEOUT_CURRENCY_MS);

        LOG_DEBUG("Attempt " + String(attempts + 1) + "/" + String(maxAttempts) + " – opening connection");
        bool connectionStarted = http.begin(client, path);

        if (connectionStarted) {
            http.addHeader(F("Authorization"), String("Bearer ") + String(bearerToken));
            http.addHeader(F("Content-Type"), F("application/json"));
            LOG_DEBUG("HTTP connection started, sending GET request...");
            yield();

            String payload = performGetRequest(http);
            http.end();
            client.stop();
            yield();

            if (payload.length() > 0) {
                value = parseFn(payload);
                if (value > 0.0f) {
                    success = true;
                    LOG_DEBUG("Data retrieved successfully: " + String(value, 2));
                } else {
                    LOG_WARNING("Parsed value invalid (attempt " + String(attempts + 1) + "/" + String(maxAttempts) + ")");
                }
            } else {
                LOG_WARNING("Empty payload (attempt " + String(attempts + 1) + "/" + String(maxAttempts) + ")");
            }
        } else {
            LOG_ERROR("Failed to begin HTTP connection (attempt " + String(attempts + 1) + "/" + String(maxAttempts) + ")");
            LOG_DEBUG("WiFi status: " + String(WiFi.status()) + ", RSSI: " + String(WiFi.RSSI()));
            client.stop();
            yield();
        }

        if (!success) {
            attempts++;
            if (attempts < maxAttempts) {
                LOG_WARNING("Retrying request (" + String(attempts) + "/" + String(maxAttempts) + ")...");
                delay(Timing::RETRY_DELAY_MS);
                yield();
            } else {
                ErrorHandler::handleError(ErrorHandler::ERROR_API,
                                          "Rate fetch failed after " + String(maxAttempts) +
                                              " attempts, keeping previous value",
                                          attempts, maxAttempts);
            }
        }
    }

    return value;
}

/** Draw a formatted currency/crypto value to the screen. */
static void displayValue(const char *symbol, float value, int decimals = 2) {
    if (value > 0) {
        drawString(String(symbol) + String(value, decimals));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CurrencyManager – public API
// ─────────────────────────────────────────────────────────────────────────────

CurrencyManager::CurrencyManager()
    : bearerTokenCurrency(confBearerTokenCurrency),
      bearerTokenCrypto(confBearerTokenCurrency),
      pathCurrencyUSD(confPathCurrencyUSD),
      pathCurrencyEUR(confPathCurrencyEUR),
      pathCryptoBTC(confPathCryptoBTC),
      dataUSDValue(0.0f), dataEURValue(0.0f), dataBTCValue(0.0f) {
    // const char* pointers assigned directly – no String copies, saves RAM
}

void CurrencyManager::initialize() {
    if (float v = readCurrency(pathCurrencyUSD); v > 0) {
        if (dataMutex != NULL) xSemaphoreTake(dataMutex, portMAX_DELAY);
        dataUSDValue = v;
        if (dataMutex != NULL) xSemaphoreGive(dataMutex);
    }
    yield();

    if (float v = readCurrency(pathCurrencyEUR); v > 0) {
        if (dataMutex != NULL) xSemaphoreTake(dataMutex, portMAX_DELAY);
        dataEURValue = v;
        if (dataMutex != NULL) xSemaphoreGive(dataMutex);
    }
    yield();

    if (float v = readCrypto(pathCryptoBTC); v > 0) {
        if (dataMutex != NULL) xSemaphoreTake(dataMutex, portMAX_DELAY);
        dataBTCValue = v;
        if (dataMutex != NULL) xSemaphoreGive(dataMutex);
    }
}

void CurrencyManager::displayUSDToScreen() {
    displayValue("$ ", dataUSDValue);
}

void CurrencyManager::displayEURToScreen() {
    displayValue("\x84 ", dataEURValue);
}

void CurrencyManager::displayBTCToScreen() {
    displayValue("B$ ", dataBTCValue, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// CurrencyManager – private methods
// ─────────────────────────────────────────────────────────────────────────────

float CurrencyManager::readCurrency(const char *path) {
    return fetchWithRetry(path, bearerTokenCurrency, parseCurrencyJson);
}

float CurrencyManager::readCrypto(const char *path) {
    return fetchWithRetry(path, bearerTokenCrypto, parseCryptoJson);
}
