#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>

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
    // Member variables to store weather data
    int temperature;
    int feelsLikeTemp;  // OpenWeatherMap "feels_like", not a daily maximum
    int pressure;
    int main_ext_humidity;
    String description_weather;
};

#endif // WEATHER_MANAGER_H
