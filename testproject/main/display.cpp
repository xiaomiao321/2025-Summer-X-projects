#include "display.h"
#include "img.h"

TFT_eSPI tft;

void initTFT() {
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(350);

  tft.init();
  tft.setRotation(1); // Horizontal 240x240
  tft.fillScreen(BG_COLOR);
  delay(100);
  tft.fillScreen(TFT_WHITE);
  delay(300);
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.drawString("Boot...", tft.width() / 2, tft.height() / 2);
  delay(1000);
}

void drawStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.pushImage(LOGO_X, LOGO_Y_TOP, 40, 40, NVIDIAGEFORCE);
  tft.pushImage(LOGO_X, LOGO_Y_BOTTOM, 40, 40, intelcore);
  drawMenu();
}

void drawMenu() {
  tft.fillRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Time", MENU_X + 10, MENU_Y + 5);
  tft.drawString("Performance", MENU_X + 80, MENU_Y + 5);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(">", MENU_X + (activeTab == 0 ? 0 : 70), MENU_Y + 5);
}

void drawSingleBar(int y, uint32_t current, uint32_t total) {
  tft.drawRect(BAR_X, y, BAR_LENGTH, BAR_HEIGHT, TFT_DARKGREY);
  int progress = (current * BAR_LENGTH) / total;
  if (progress > 0) {
    uint16_t color = (progress >= BAR_LENGTH) ? TFT_GREEN : VALUE_COLOR;
    tft.fillRect(BAR_X + 1, y + 1, progress - 1, BAR_HEIGHT - 1, color);
  }
}

void drawTimeTab() {
  tft.fillRect(DATA_X, DATA_Y, 180, 200, BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("ESP32 monitor", DATA_X, DATA_Y);
  tft.drawString("Time:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("AHT sensor:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("Weather:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("ESP32 Temp:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);

  // Draw time
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %a", &timeinfo);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(buf, DATA_X, DATA_Y + LINE_HEIGHT);
  sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.drawString(buf, DATA_X + 60, DATA_Y + LINE_HEIGHT);

  // Draw AHT sensor
  String ahtStr = String(aht_temp, 1) + "°C  " + String(aht_hum, 1) + "%";
  tft.drawString(ahtStr, DATA_X, DATA_Y + 2 * LINE_HEIGHT);

  // Draw weather
  String weatherStr = weather_temp + "°C  " + weather_hum + "%";
  tft.drawString(weatherStr, DATA_X, DATA_Y + 3 * LINE_HEIGHT);

  // Draw ESP32 temperature
  tft.drawString(String(esp32c3_temp, 1) + "°C", DATA_X, DATA_Y + 4 * LINE_HEIGHT);

  // Draw progress bars
  uint32_t secOfDay = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  uint16_t secOfHour = timeinfo.tm_min * 60 + timeinfo.tm_sec;
  tft.fillRect(BAR_X, BAR_DAY_Y, BAR_LENGTH, 3 * BAR_HEIGHT + 2 * BAR_MARGIN, BG_COLOR);
  drawSingleBar(BAR_DAY_Y, secOfDay, DAY_SECOND);
  drawSingleBar(BAR_HOUR_Y, secOfHour, 3600);
  drawSingleBar(BAR_MINUTE_Y, timeinfo.tm_sec, 60);
}

void drawPerformanceTab() {
  tft.fillRect(DATA_X, DATA_Y, 180, 200, BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("PC data:", DATA_X, DATA_Y);
  tft.drawString("CPU name:", DATA_X + 10, DATA_Y + LINE_HEIGHT);
  tft.drawString("CPU temp:", DATA_X + 10, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("CPU load:", DATA_X + 10, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("GPU name:", DATA_X + 10, DATA_Y + 4 * LINE_HEIGHT);
  tft.drawString("GPU temp:", DATA_X + 10, DATA_Y + 5 * LINE_HEIGHT);
  tft.drawString("GPU load:", DATA_X + 10, DATA_Y + 6 * LINE_HEIGHT);
  tft.drawString("RAM load:", DATA_X + 10, DATA_Y + 7 * LINE_HEIGHT);

  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  if (xSemaphoreTake(xPCDataMutex, 10) == pdTRUE) {
    if (pcData.valid) {
      tft.drawString(String(pcData.cpuName), DATA_X + 80, DATA_Y + LINE_HEIGHT);
      tft.drawString(String(pcData.cpuTemp) + "°C", DATA_X + 80, DATA_Y + 2 * LINE_HEIGHT);
      tft.drawString(String(pcData.cpuLoad) + "%", DATA_X + 80, DATA_Y + 3 * LINE_HEIGHT);
      tft.drawString(String(pcData.gpuName), DATA_X + 80, DATA_Y + 4 * LINE_HEIGHT);
      tft.drawString(String(pcData.gpuTemp) + "°C", DATA_X + 80, DATA_Y + 5 * LINE_HEIGHT);
      tft.drawString(String(pcData.gpuLoad) + "%", DATA_X + 80, DATA_Y + 6 * LINE_HEIGHT);
      tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + 80, DATA_Y + 7 * LINE_HEIGHT);
    } else {
      tft.setTextColor(ERROR_COLOR, BG_COLOR);
      tft.drawString("No data", DATA_X + 80, DATA_Y + LINE_HEIGHT);
    }
    xSemaphoreGive(xPCDataMutex);
  }
}

void showError(const char* msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawStaticElements();
  if (activeTab == 0) drawTimeTab();
  else drawPerformanceTab();
}

void TFT_Init_Task(void *pvParameters) {
  initTFT();
  drawStaticElements();
  drawTimeTab();
  vTaskDelete(NULL);
}

void TFT_Task(void *pvParameters) {
  int lastTab = -1;
  int lastSec = -1, lastMin = -1;
  static time_t last_time_update = 0; 
  for (;;) {
    getLocalTime(&timeinfo);

    if (activeTab != lastTab) {
      drawMenu();
      if (activeTab == 0) drawTimeTab();
      else drawPerformanceTab();
      lastTab = activeTab;
    }

    if (activeTab == 0 && timeinfo.tm_sec != lastSec) {
      drawTimeTab();
      lastSec = timeinfo.tm_sec;
    }

    if (activeTab == 0 && (timeinfo.tm_min != lastMin || millis() % 5000 < 100)) {
      drawTimeTab();
      lastMin = timeinfo.tm_min;
    }

    if (activeTab == 1 && (millis() % 5000 < 100)) {
      drawPerformanceTab();
    }

    // Periodic time sync
    if (time(nullptr) - last_time_update >= TIME_SYNC_INTERVAL_S) {
      xTaskCreatePinnedToCore(WIFI_Init_Task, "WiFi_Init", 8192, NULL, 2, NULL, 0);
      last_time_update = time(nullptr);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}