#pragma once

#include <WiFi.h>

class WIFISetup
{
  public:
      // Function declarations for WiFi setup
      void wifi_init();

      // Check WiFi connection status
      static bool isConnected();

      // Attempt to reconnect to saved WiFi
      bool attemptReconnect();
      
  private:
      // Attempt direct connection to saved WiFi credentials
      bool attemptDirectConnection(int maxAttempts);
      
      // Check if WiFi credentials are saved
      bool hasSavedCredentials();
};
