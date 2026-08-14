#include "MQTTClient.h"
#include "dht22_manager.h"
#include "clock.h"
#include "logger.h"
#include "constants.h"

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() {
  int attemptCount = 0;
  const int maxAttempts = Retry::MAX_ATTEMPTS_MQTT;

  while (!client.connected() && attemptCount < maxAttempts) {
    LOG_DEBUG("Attempting MQTT connection (attempt " + String(attemptCount + 1) + "/" + String(maxAttempts) + ")...");
    if (client.connect(hostname_m.c_str(), mqtt_user, mqtt_password)) {
      LOG_INFO("MQTT connected successfully");
      return;
    } else {
      LOG_WARNING("MQTT connection failed, rc=" + String(client.state()) + ", retrying...");
      delay(Timing::MQTT_RECONNECT_DELAY_MS);
      attemptCount++;
    }
  }

  if (attemptCount >= maxAttempts) {
    LOG_ERROR_F("Max MQTT connection attempts reached. Could not connect to broker.");
  }
}


void setup_mqtt() {
  LOG_INFO("MQTT broker: " + String(mqtt_server) + ":1883");
  client.setServer(mqtt_server, 1883);

  // client.loop() only runs once per data-update cycle. With PubSubClient's
  // 15s default keepalive the broker would drop us between every cycle, so the
  // "reconnect if needed" path became "reconnect every time". Widen the
  // keepalive past the cycle interval instead.
  client.setKeepAlive(NetworkConfig::MQTT_KEEPALIVE_SEC);
  client.setSocketTimeout(NetworkConfig::MQTT_SOCKET_TIMEOUT_SEC);

  LOG_INFO("MQTT topic: " + mqtt_topic_str);
}

void publish_temperature() {
  // Without a sensor homeTemp is still at its 0.0 initialiser, which is a
  // plausible-looking reading - don't publish it as though it were measured.
  if (!IS_DHT_CONNECTED) {
    LOG_DEBUG_F("No DHT sensor on this device, skipping temperature publish");
    return;
  }

  Dht22_manager& dht22_manager = Clock::getInstance().getDht22();
  float temperature = dht22_manager.getHomeTemp();

  if (isnan(temperature)) {
    LOG_ERROR_F("Failed to read temperature from DHT sensor!");
    return;
  }

  LOG_DEBUG("Publishing temperature: " + String(temperature, 2) + "°C");

  char tempString[Buffer::TEMP_STRING_SIZE];
  dtostrf(temperature, 1, 2, tempString);
  
  if (client.publish(mqtt_topic_str.c_str(), tempString)) {
    LOG_VERBOSE("Temperature published to MQTT successfully");
  } else {
    LOG_WARNING_F("Failed to publish temperature to MQTT");
  }
}
