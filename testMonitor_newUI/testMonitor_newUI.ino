#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// -----------------------------
// 配置区
// -----------------------------
#define SENSOR_INTERVAL_MS 5000
#define CHART_X 3
#define CHART_Y 80
#define CHART_WIDTH 122
#define CHART_HEIGHT 45
#define MAX_DATA_POINTS 10
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

// -----------------------------
// 全局变量
// -----------------------------
TFT_eSPI tft = TFT_eSPI();
Adafruit_AHTX0 aht;
float aht_temp = 0.0f, aht_hum = 0.0f;
bool aht_ok = false;
float esp32c3_temp = 0.0f;
struct PCData {
  int cpuLoad;
  int gpuLoad;
  float ramLoad;
  bool valid;
} pcData = {.cpuLoad = 0, .gpuLoad = 0, .ramLoad = 0.0, .valid = false};
struct PerformanceHistory {
  int cpuLoadHistory[MAX_DATA_POINTS];
  int gpuLoadHistory[MAX_DATA_POINTS];
  float ramLoadHistory[MAX_DATA_POINTS];
  int currentIndex;
  int count;
} perfHistory = {
    .cpuLoadHistory = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100}, // Initial data for testing
    .gpuLoadHistory = {15, 25, 35, 45, 55, 65, 75, 85, 95, 100},
    .ramLoadHistory = {5.0, 15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0, 95.0},
    .currentIndex = 0,
    .count = MAX_DATA_POINTS
};
SemaphoreHandle_t xPCDataMutex = NULL;

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawPerformanceStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Performance", DATA_X, DATA_Y);
  tft.drawString("CPU:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("GPU:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("RAM:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("ESP Temp:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);
}

// -----------------------------
// 绘制折线图
// -----------------------------
void drawPerformanceChart() {
  // Clear chart area
  tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, BG_COLOR);
  
  // Draw chart border
  tft.drawRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, TFT_WHITE);

  // Draw grid lines (horizontal, 25% increments)
  for (int i = 0; i <= 4; i++) {
    int y = CHART_Y + (CHART_HEIGHT * i / 4);
    tft.drawFastHLine(CHART_X, y, CHART_WIDTH, TFT_DARKGREY);
  }

  // Draw data points
  int pointsToDraw = min(perfHistory.count, MAX_DATA_POINTS);
  if (pointsToDraw < 2) return; // Need at least 2 points to draw lines

  int startIndex = perfHistory.count < MAX_DATA_POINTS ? 0 : perfHistory.currentIndex;
  float xStep = (float)CHART_WIDTH / (MAX_DATA_POINTS - 1);

  // Draw CPU Load (Cyan)
  for (int i = 0; i < pointsToDraw - 1; i++) {
    int index = (startIndex + i) % MAX_DATA_POINTS;
    int nextIndex = (startIndex + i + 1) % MAX_DATA_POINTS;
    float value1 = max(0.0f, min(100.0f, (float)perfHistory.cpuLoadHistory[index]));
    float value2 = max(0.0f, min(100.0f, (float)perfHistory.cpuLoadHistory[nextIndex]));
    int y1 = CHART_Y + CHART_HEIGHT - (int)(value1 * CHART_HEIGHT / 100.0);
    int y2 = CHART_Y + CHART_HEIGHT - (int)(value2 * CHART_HEIGHT / 100.0);
    int x1 = CHART_X + (int)(i * xStep);
    int x2 = CHART_X + (int)((i + 1) * xStep);
    tft.drawLine(x1, y1, x2, y2, TFT_CYAN);
    Serial.printf("CPU Line: (%d, %d) to (%d, %d)\n", x1, y1, x2, y2);
  }

  // Draw GPU Load (Yellow)
  for (int i = 0; i < pointsToDraw - 1; i++) {
    int index = (startIndex + i) % MAX_DATA_POINTS;
    int nextIndex = (startIndex + i + 1) % MAX_DATA_POINTS;
    float value1 = max(0.0f, min(100.0f, (float)perfHistory.gpuLoadHistory[index]));
    float value2 = max(0.0f, min(100.0f, (float)perfHistory.gpuLoadHistory[nextIndex]));
    int y1 = CHART_Y + CHART_HEIGHT - (int)(value1 * CHART_HEIGHT / 100.0);
    int y2 = CHART_Y + CHART_HEIGHT - (int)(value2 * CHART_HEIGHT / 100.0);
    int x1 = CHART_X + (int)(i * xStep);
    int x2 = CHART_X + (int)((i + 1) * xStep);
    tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    Serial.printf("GPU Line: (%d, %d) to (%d, %d)\n", x1, y1, x2, y2);
  }

  // Draw RAM Load (Red)
  for (int i = 0; i < pointsToDraw - 1; i++) {
    int index = (startIndex + i) % MAX_DATA_POINTS;
    int nextIndex = (startIndex + i + 1) % MAX_DATA_POINTS;
    float value1 = max(0.0f, min(100.0f, perfHistory.ramLoadHistory[index]));
    float value2 = max(0.0f, min(100.0f, perfHistory.ramLoadHistory[nextIndex]));
    int y1 = CHART_Y + CHART_HEIGHT - (int)(value1 * CHART_HEIGHT / 100.0);
    int y2 = CHART_Y + CHART_HEIGHT - (int)(value2 * CHART_HEIGHT / 100.0);
    int x1 = CHART_X + (int)(i * xStep);
    int x2 = CHART_X + (int)((i + 1) * xStep);
    tft.drawLine(x1, y1, x2, y2, TFT_RED);
    Serial.printf("RAM Line: (%d, %d) to (%d, %d)\n", x1, y1, x2, y2);
  }

  // Draw legend
  tft.setTextColor(TFT_CYAN, BG_COLOR);
  tft.drawString("CPU", CHART_X, CHART_Y - 12);
  tft.setTextColor(TFT_YELLOW, BG_COLOR);
  tft.drawString("GPU", CHART_X + 30, CHART_Y - 12);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.drawString("RAM", CHART_X + 60, CHART_Y - 12);
}

// -----------------------------
// 更新性能历史数据
// -----------------------------
void updatePerformanceHistory() {
  if (xSemaphoreTake(xPCDataMutex, 10) == pdTRUE) {
    if (pcData.valid) {
      perfHistory.cpuLoadHistory[perfHistory.currentIndex] = pcData.cpuLoad;
      perfHistory.gpuLoadHistory[perfHistory.currentIndex] = pcData.gpuLoad;
      perfHistory.ramLoadHistory[perfHistory.currentIndex] = pcData.ramLoad;
      perfHistory.currentIndex = (perfHistory.currentIndex + 1) % MAX_DATA_POINTS;
      if (perfHistory.count < MAX_DATA_POINTS) {
        perfHistory.count++;
      }
      Serial.printf("Updated History: CPU=%d, GPU=%d, RAM=%.1f, Index=%d, Count=%d\n",
                    pcData.cpuLoad, pcData.gpuLoad, pcData.ramLoad, perfHistory.currentIndex, perfHistory.count);
    }
    xSemaphoreGive(xPCDataMutex);
  }
}

// -----------------------------
// 更新性能数据
// -----------------------------
void updatePerformanceData() {
  if (xSemaphoreTake(xPCDataMutex, 10) != pdTRUE)
    return;
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  if (pcData.valid) {
    tft.fillRect(DATA_X + 40, DATA_Y + LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuLoad) + "%", DATA_X + 40, DATA_Y + LINE_HEIGHT);
    tft.fillRect(DATA_X + 40, DATA_Y + 2 * LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.gpuLoad) + "%", DATA_X + 40, DATA_Y + 2 * LINE_HEIGHT);
    tft.fillRect(DATA_X + 40, DATA_Y + 3 * LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + 40, DATA_Y + 3 * LINE_HEIGHT);
  } else {
    tft.fillRect(DATA_X + 40, DATA_Y + LINE_HEIGHT, 80, 3 * LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(ERROR_COLOR, BG_COLOR);
    tft.drawString("No Data", DATA_X + 40, DATA_Y + LINE_HEIGHT);
  }
  tft.fillRect(DATA_X + 60, DATA_Y + 4 * LINE_HEIGHT, 60, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(String(esp32c3_temp, 1) + "°C", DATA_X + 60, DATA_Y + 4 * LINE_HEIGHT);
  updatePerformanceHistory();
  drawPerformanceChart();
  xSemaphoreGive(xPCDataMutex);
}

// -----------------------------
// 显示错误信息
// -----------------------------
void showPerformanceError(const char *msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawPerformanceStaticElements();
}

// -----------------------------
// 模拟 PC 数据
// -----------------------------
void simulatePCData(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
      pcData.cpuLoad = random(0, 101); // 0-100%
      pcData.gpuLoad = random(0, 101);
      pcData.ramLoad = random(0, 1010) / 10.0; // 0.0-100.0%
      pcData.valid = true;
      Serial.printf("Simulated Data: CPU=%d, GPU=%d, RAM=%.1f\n", 
                    pcData.cpuLoad, pcData.gpuLoad, pcData.ramLoad);
      xSemaphoreGive(xPCDataMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second
  }
}

// -----------------------------
// AHT 传感器初始化任务
// -----------------------------
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

// -----------------------------
// AHT 传感器读取任务
// -----------------------------
void AHT_Task(void *pvParameters) {
  for (;;) {
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        aht_temp = temp.temperature;
        aht_hum = hum.relative_humidity;
        Serial.printf("AHT Data: Temp=%.1f°C, Hum=%.1f%%\n", aht_temp, aht_hum);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
  }
}

// -----------------------------
// 性能监控显示任务
// -----------------------------
void Performance_Task(void *pvParameters) {
  for (;;) {
    esp32c3_temp = temperatureRead();
    updatePerformanceData();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second
  }
}

// -----------------------------
// 主任务
// -----------------------------
void Performance_Main_Task(void *pvParameters) {
  drawPerformanceStaticElements();
  xPCDataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(AHT_Init_Task, "AHT_Init", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(AHT_Task, "AHT_Read", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(simulatePCData, "Simulate_Data", 2048, NULL, 1, NULL, 0);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000)); // Keep task alive
  }
}

// -----------------------------
// setup() & loop()
// -----------------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1); // 横屏 128x128
  tft.fillScreen(BG_COLOR);
  xTaskCreatePinnedToCore(Performance_Main_Task, "Perf_Main", 8192, NULL, 2, NULL, 0);
}

void loop() {
  delay(1000); // Empty loop, as tasks handle everything
}