#include "sensors.h"
#include "network.h"

Adafruit_AHTX0 aht;
float aht_temp = 0.0f, aht_hum = 0.0f;
String weather_main = "--";
String weather_temp = "--";
String weather_hum = "--";
bool aht_ok = false;
unsigned long last_weather_update = 0;

void AHT_Init_Task(void *pvParameters) {
  Wire.begin(20, 21);
  Serial.println("Initializing AHT...");
  aht_ok = aht.begin();
  if (aht_ok)
    Serial.println("✅ AHT sensor found!");
  else
    Serial.println("❌ AHT sensor not found!");
  vTaskDelete(NULL);
}

void AHT_Task(void *pvParameters) {
  for (;;) {
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        aht_temp = temp.temperature;
        aht_hum = hum.relative_humidity;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
  }
}