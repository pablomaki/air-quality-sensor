#include "DHT.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ESP32 deep sleep settings
#define SLEEP_TIME_US 60000000

//DHT Settings
#define DHTPIN 4
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Debug
const bool debug = true;

// WiFi setup
// const char* ssid = "Thom_D0041089";
// const char* password = "0bafeb650ecdf337b5e6204c72";
const char* ssid = "ZyXEL_BFA1";
const char* password = "HHKG3YPEJH77P";

// MQTT broker details
const char* mqtt_server = "192.168.0.21"; // Homebridge RPi IP 
const int mqtt_port = 1883; // Default MQTT port
const char* client_name = "sensor_1/esp32client";
const char* mqtt_user = "admin"; // Optional (use if required by your broker)
const char* mqtt_password = "Hyttyn3n"; // Optional (use if required by your broker)

// MQTT topics
const char* pub_air_quality_topic = "sensor_1/air_quality";
const char* pub_temperature_topic = "sensor_1/temperature";
const char* pub_relative_humidity_topic = "sensor_1/relative_humidity";

// Air quality constants
const float ideal_temperature = 20.0;  // Ideal temperature in Celsius
const float temp_tolerance = 0.5;      // Acceptable deviation from the ideal temperature
const float ideal_humidity = 45.0;     // Ideal humidity percentage
const float humidity_tolerance = 5.0;  // Acceptable deviation from the ideal humidity

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

const bool setup_wifi()
{
  WiFi.begin(ssid, password);
  uint8_t count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 20) {
    delay(1000);
    count++;
  }
  return (WiFi.status() != WL_CONNECTED) ? false : true;
}

// Connect to the mqtt broker and subscribe to the topics
const bool setup_mqtt()
{
  client.setServer(mqtt_server, mqtt_port);
  uint8_t count = 0;
  client.connect(client_name, mqtt_user, mqtt_password);
  while (!client.connected() && count < 20) {
    if (client.connect(client_name, mqtt_user, mqtt_password)) 
    {
      client.subscribe(pub_air_quality_topic);
      client.subscribe(pub_temperature_topic);
      client.subscribe(pub_relative_humidity_topic);
    }
    delay(1000);
    count++;
  }
  return (client.connected()) ? true : false;
}

char* getAirQuality(float* humidity_ptr, float* temperature_ptr) {
    // Validate inputs
    if (*temperature_ptr < -50 || *temperature_ptr > 50 || *humidity_ptr < 0 || *humidity_ptr > 100) 
    {
        return "UNKNOWN";
    }

    // Calculate the difference from ideal conditions
    float temp_diff = abs(*temperature_ptr - ideal_temperature);
    float humidity_diff = abs(*humidity_ptr - ideal_humidity);

    // Determine the condition based on differences
    if (temp_diff <= temp_tolerance && humidity_diff <= humidity_tolerance) 
    {
        return "EXCELLENT";
    } 
    else if (temp_diff <= temp_tolerance * 1.5 && humidity_diff <= humidity_tolerance * 1.5) 
    {
        return "GOOD";
    } 
    else if (temp_diff <= temp_tolerance * 2 && humidity_diff <= humidity_tolerance * 2) 
    {
        return "FAIR";
    } 
    else if (temp_diff <= temp_tolerance * 3 && humidity_diff <= humidity_tolerance * 3) 
    {
        return "INFERIOR";
    } 
    else {
        return "POOR";
    }
}

const bool read_sensor_data(float* humidity_ptr, float* temperature_ptr, char** air_quality_ptr)
{
    // Read sensor data
  *humidity_ptr = dht.readHumidity();
  *temperature_ptr = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(*humidity_ptr) || isnan(*temperature_ptr)) {
    *humidity_ptr = ideal_humidity;
    *temperature_ptr = ideal_temperature;
    // return false;
  }

  // Get the air quality value
  const char* air_quality = getAirQuality(humidity_ptr, temperature_ptr);
  *air_quality_ptr = new char[strlen(air_quality) + 1];
  strcpy(*air_quality_ptr, air_quality);
  return true;
}

const bool publish_data(float* humidity_ptr, float* temperature_ptr, char** air_quality_ptr)
{
  if (!client.publish(pub_air_quality_topic, *air_quality_ptr) ||
      !client.publish(pub_temperature_topic, String(*temperature_ptr).c_str()) ||
      !client.publish(pub_relative_humidity_topic, String(*humidity_ptr).c_str()))
  {
    return false;
  }
  return true;
}

void setup() {
  // Initialize serial for debugging 
  if (debug)
  {
    Serial.begin(9600);
  }

  // Initialiuze the DHT sensor
  dht.begin();

  // Initialize Wi-Fi connection
  if (debug)
  {
    Serial.print("Connecting to Wi-Fi: ");
    Serial.print(ssid);
    Serial.println();
  }
  bool success = setup_wifi();
  if (!success)
  {
    if (debug)
    {
      Serial.println("Failed to connect to Wi-Fi");
    }
    return;
  }

  if (debug)
  {
    Serial.print("WiFi connected, IP address: ");
    Serial.print(WiFi.localIP());
    Serial.print(", Gateway address: ");
    Serial.print(WiFi.gatewayIP());
    Serial.println();
  }

  // Initialize MQTT connection
  if (debug)
  {
    Serial.println("Connecting to MQTT broker");
  }
  success = setup_mqtt();
  if (!success)
  {
    if (debug)
    {
      Serial.println("Failed to connect to MQTT broker");
    }
    return;
  }
  if (debug)
  {
    Serial.println("Connected to the MQTT broker");
  }

  // Get air quality dataform the sensor
  float humidity, temperature;
  char* air_quality = nullptr;
  success = read_sensor_data(&humidity, &temperature, &air_quality);
  if (!success)
  {
    if (debug)
    {
      Serial.println("Sensor data and publish failed.");
    }
  }
  if (debug)
  {
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(temperature);
    Serial.print(F("°C "));
    Serial.print(F("Air quality: "));
    Serial.print(air_quality);
    Serial.println("");
  }

  // Publish the data
  success = publish_data(&humidity, &temperature, &air_quality);
  if (!success)
  {
    if (debug)
    {
      Serial.println("Sensor data publishing failed.");
    }
  }
  if (debug)
  {
    Serial.println("Succesfully published the data to MQTT broker.");
  }

  // Enter deep sleep
  sleep(1);
  if (debug)
  {
    Serial.println("Enter sleep mode.");
  }
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US - micros());
  esp_deep_sleep_start();

}

void loop() 
{  
}
