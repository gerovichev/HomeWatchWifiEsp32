#pragma once

#include <Arduino.h>
#include "global_config.h"

/**
 * Read access to the per-device configuration resolved at boot by
 * initPerDevice() from the board's MAC address.
 *
 * This is the single intended way to read that configuration. The underlying
 * globals in global_config.h remain only because initPerDevice() and a few
 * legacy display helpers write them directly; new code should go through here.
 *
 * There are no setters: the values are decided once during setup() and are
 * read from both cores afterwards, so making them mutable at runtime would
 * introduce a data race for no benefit.
 */
class DeviceState {
public:
    static DeviceState& getInstance();

    const String& getLanguage() const { return lang_weather; }
    const String& getHostname() const { return hostname_m; }
    const String& getWatchName() const { return nameofWatch; }
    const String& getMacAddress() const { return macAddrSt; }

    bool isDhtConnected() const { return IS_DHT_CONNECTED; }
    bool isWebClientNeeded() const { return ::isWebClientNeeded; }
    bool isReadWeather() const { return ::isReadWeather; }
    bool isOtaRequired() const { return isOTAreq; }
    bool isMqttEnabled() const { return isMQTT; }

    float getHumidityDelta() const { return humidity_delta; }

    const String* getDaysOfWeek() const { return daysOfTheWeek; }

private:
    DeviceState() = default;
    DeviceState(const DeviceState&) = delete;
    DeviceState& operator=(const DeviceState&) = delete;
};

