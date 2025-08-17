#include "network.h"
#include "sensors.h"
String net_weather_main = "--";
String net_weather_temp = "--";
String net_weather_hum = "--";
bool wifi_connected = false;
unsigned long last_time_update = 0;
struct tm timeinfo;

bool connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long timeout = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout)
    delay(500);
    wifi_connected = (WiFi.status() == WL_CONNECTED);
  if (wifi_connected) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
  }
  return wifi_connected;
}

void disconnectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifi_connected = false;
}

bool syncNTPTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
  return getLocalTime(&timeinfo, 10000);
}

bool getWeather() {
  HTTPClient http;
  http.begin(WEATHER_API_URL);
  int code = http.GET();
  bool success = false;

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    JsonArray lives = doc["lives"];
    if (lives.size() > 0) {
      net_weather_main = lives[0]["weather"].as<String>();
      net_weather_temp = lives[0]["temperature"].as<String>();
      net_weather_hum = lives[0]["humidity"].as<String>();
      success = true;
    }
  }
  http.end();
  return success;
}

void WIFI_Init_Task(void *pvParameters) {
  if (connectWiFi()) {
    if (syncNTPTime()) {
      Serial.println("✅ Time synchronized");
      last_time_update = time(nullptr);
    }
    if (getWeather()) {
      Serial.println("✅ Weather data acquired");
      last_weather_update = time(nullptr);
    }
    disconnectWiFi();
  }
  vTaskDelete(NULL);
}