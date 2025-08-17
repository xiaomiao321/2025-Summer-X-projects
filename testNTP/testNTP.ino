
#include <WiFi.h>
#include "time.h"
const char* ssid     = "xiaomiao";
const char* password = "xiaomiao123";

// 使用阿里云 NTP 服务，适合中国用户
const char* ntpServer = "ntp.aliyun.com";
const long  gmtOffset_sec = 8 * 3600;    // UTC+8 北京时间
const int   daylightOffset_sec = 0;      // 无夏令时

void printLocalTime()
{
   struct tm timeinfo;
   if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
   }
   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
   //Serial2.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.println("Waiting for time sync...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  printLocalTime(); // 打印同步后的时间

  // 若无需持续联网，可关闭 Wi-Fi
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
}

void loop() {
  delay(1000);
  printLocalTime();
}