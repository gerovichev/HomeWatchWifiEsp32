#include "currency_manager.h"
#include "constants.h"
#include "logger.h"
#include "http_fetch.h"
#include "data_lock.h"
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────────────────────
// File-local helper functions
// ─────────────────────────────────────────────────────────────────────────────

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
 * Supports the Home Assistant sensor format {"state": <value>}
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
    fetchInto(pathCurrencyUSD, bearerTokenCurrency, parseCurrencyJson, "USD", dataUSDValue);
    yield();
    fetchInto(pathCurrencyEUR, bearerTokenCurrency, parseCurrencyJson, "EUR", dataEURValue);
    yield();
    fetchInto(pathCryptoBTC, bearerTokenCrypto, parseCryptoJson, "BTC", dataBTCValue);
}

/**
 * Fetch one rate and store it. A parsed value of zero or less is treated as a
 * failed attempt (the shared fetcher retries), and on total failure `target`
 * keeps whatever it held before.
 */
void CurrencyManager::fetchInto(const char *path, const char *token,
                                float (*parseFn)(const String &),
                                const char *label, float &target) {
    HttpFetchOptions opts;
    opts.url = path;
    opts.tag = label;
    opts.bearerToken = token;
    opts.timeoutMs = Timing::HTTP_TIMEOUT_CURRENCY_MS;
    opts.maxAttempts = Retry::MAX_ATTEMPTS_CURRENCY;

    httpFetchWithRetry(opts, [&](const String &payload) {
        const float value = parseFn(payload);
        if (value <= 0.0f) {
            LOG_WARNINGF("%s: parsed value invalid", label);
            return false;
        }
        {
            DataLock lock;
            target = value;
        }
        LOG_DEBUGF("%s retrieved successfully: %.2f", label, value);
        return true;
    });
}

float CurrencyManager::valueOf(const float &field) const {
    DataLock lock;
    return field;
}

void CurrencyManager::displayUSDToScreen() {
    displayValue("$ ", valueOf(dataUSDValue));
}

void CurrencyManager::displayEURToScreen() {
    displayValue("\x84 ", valueOf(dataEURValue));
}

void CurrencyManager::displayBTCToScreen() {
    displayValue("B$ ", valueOf(dataBTCValue), 0);
}
