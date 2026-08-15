#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>

// Plain copy of the weather fields, handed out under dataMutex so the display
// core can format and draw without holding the lock.
struct WeatherSnapshot {
    int temperature = 0;
    int feelsLike = 0;   // OpenWeatherMap "feels_like", not a daily maximum
    int pressure = 0;    // mmHg
    int humidity = 0;
    char description[96] = {0};  // Fixed buffer: no heap allocation under lock
};

class WeatherManager {
public:
    // Constructor
    WeatherManager();

    // Methods to interact with weather data
    void readWeather();                       // Reads weather data (replace with real data fetch)
    void printWeatherToScreen() const;        // Prints all weather data
    void printFeelsLikeToScreen() const;      // Prints apparent ("feels like") temperature
    void printPressureToScreen() const;       // Prints pressure
    void printHumidityToScreen() const;       // Prints humidity
    void printDescriptionWeatherToScreen() const; // Prints weather description

private:
    // Copies the current readings under dataMutex.
    WeatherSnapshot snapshot() const;

    // Member variables to store weather data
    WeatherSnapshot data;
};

#endif // WEATHER_MANAGER_H
