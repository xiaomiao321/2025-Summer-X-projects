#include "weather.h"
#include "menu.h"
#include "RotaryEncoder.h"
#include <sys/time.h> // For settimeofday
#include <time.h>     // For mktime
#include "Buzzer.h"

const long GMT_OFFSET_SEC = 8 * 3600; // Keep for initial time setup if needed
const int DAYLIGHT_OFFSET = 0; // Keep for initial time setup if needed

// -----------------------------
// 全局变量
// -----------------------------
struct tm timeinfo;
unsigned long lastDrinkTime = 0; // Last time water was drunk
unsigned long lastMoveTime = 0;  // Last time activity was done
const unsigned long INTERVAL_MS = 30 * 60 * 1000; // 30 minutes in milliseconds

// Progress bar constants
#define BAR_WIDTH 180
#define BAR_HEIGHT 10
#define BAR_START_Y 100 // New constant to control vertical position
#define BAR_Y_SPACING 30 // Spacing between bars

#define MINUTE_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 2)
#define HOUR_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 1)
#define DAY_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 0)
#define DRINK_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 3) // New bar for drink water
#define MOVE_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 4)  // New bar for get up and move

#define PERCENTAGE_TEXT_X (DATA_X + BAR_WIDTH + 10)

// Double click constants
#define DOUBLE_CLICK_THRESHOLD_MS 300 // Max time between clicks for double click

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawWeatherStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Time:", DATA_X, DATA_Y);
}

// -----------------------------
// 绘制时间及进度条
// -----------------------------
void drawWeatherTime() {
  char buf[32];
  // Display date
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  tft.fillRect(DATA_X + 60, DATA_Y, 180, LINE_HEIGHT, BG_COLOR); // Clear previous date area
  tft.setTextSize(1); // Smaller size for date
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(buf, DATA_X + 60, DATA_Y);

  // Display time (enlarged)
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  tft.fillRect(DATA_X + 60, DATA_Y + LINE_HEIGHT, 180, LINE_HEIGHT, BG_COLOR); // Clear previous time area
  tft.setTextSize(2); // Enlarge time display
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(buf, DATA_X + 60, DATA_Y + LINE_HEIGHT);

  // Minute progress bar
  float minute_progress = (float)timeinfo.tm_sec / 60.0;
  tft.fillRect(DATA_X, MINUTE_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY); // Bar background
  tft.fillRect(DATA_X, MINUTE_BAR_Y, (int)(BAR_WIDTH * minute_progress), BAR_HEIGHT, TFT_GREEN); // Progress
  tft.setTextSize(1); // Reset text size for percentage
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.fillRect(PERCENTAGE_TEXT_X, MINUTE_BAR_Y, 50, BAR_HEIGHT, BG_COLOR); // Clear previous percentage text
  sprintf(buf, "%.0f%%", minute_progress * 100);
  tft.drawString(buf, PERCENTAGE_TEXT_X, MINUTE_BAR_Y); // Percentage display

  // Hour progress bar
  float hour_progress = (float)(timeinfo.tm_min * 60 + timeinfo.tm_sec) / 3600.0;
  tft.fillRect(DATA_X, HOUR_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY); // Bar background
  tft.fillRect(DATA_X, HOUR_BAR_Y, (int)(BAR_WIDTH * hour_progress), BAR_HEIGHT, TFT_BLUE); // Progress
  tft.setTextSize(1); // Reset text size for percentage
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.fillRect(PERCENTAGE_TEXT_X, HOUR_BAR_Y, 50, BAR_HEIGHT, BG_COLOR); // Clear previous percentage text
  sprintf(buf, "%.0f%%", hour_progress * 100);
  tft.drawString(buf, PERCENTAGE_TEXT_X, HOUR_BAR_Y); // Percentage display

  // Day progress bar
  // Calculate seconds passed in the day
  long seconds_in_day = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  float day_progress = (float)seconds_in_day / (24.0 * 3600.0);
  tft.fillRect(DATA_X, DAY_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY); // Bar background
  tft.fillRect(DATA_X, DAY_BAR_Y, (int)(BAR_WIDTH * day_progress), BAR_HEIGHT, TFT_RED); // Progress
  tft.setTextSize(1); // Reset text size for percentage
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.fillRect(PERCENTAGE_TEXT_X, DAY_BAR_Y, 50, BAR_HEIGHT, BG_COLOR); // Clear previous percentage text
  sprintf(buf, "%.0f%%", day_progress * 100);
  tft.drawString(buf, PERCENTAGE_TEXT_X, DAY_BAR_Y); // Percentage display

  // Drink Water progress bar
  unsigned long elapsedDrinkTime = millis() - lastDrinkTime;
  float drink_progress = (float)elapsedDrinkTime / INTERVAL_MS;
  if (drink_progress > 1.0) drink_progress = 1.0; // Cap at 100%

  tft.fillRect(DATA_X, DRINK_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY); // Bar background
  tft.fillRect(DATA_X, DRINK_BAR_Y, (int)(BAR_WIDTH * drink_progress), BAR_HEIGHT, TFT_CYAN); // Progress
  tft.setTextSize(1);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.fillRect(PERCENTAGE_TEXT_X, DRINK_BAR_Y, 50, BAR_HEIGHT, BG_COLOR); // Clear previous percentage text
  sprintf(buf, "%.0f%%", drink_progress * 100);
  tft.drawString(buf, PERCENTAGE_TEXT_X, DRINK_BAR_Y); // Percentage display
  tft.drawString("Drink:", DATA_X - 40, DRINK_BAR_Y); // Label

  // Get Up and Move progress bar
  unsigned long elapsedMoveTime = millis() - lastMoveTime;
  float move_progress = (float)elapsedMoveTime / INTERVAL_MS;
  if (move_progress > 1.0) move_progress = 1.0; // Cap at 100%

  tft.fillRect(DATA_X, MOVE_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY); // Bar background
  tft.fillRect(DATA_X, MOVE_BAR_Y, (int)(BAR_WIDTH * move_progress), BAR_HEIGHT, TFT_MAGENTA); // Progress
  tft.setTextSize(1);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.fillRect(PERCENTAGE_TEXT_X, MOVE_BAR_Y, 50, BAR_HEIGHT, BG_COLOR); // Clear previous percentage text
  sprintf(buf, "%.0f%%", move_progress * 100);
  tft.drawString(buf, PERCENTAGE_TEXT_X, MOVE_BAR_Y); // Percentage display
  tft.drawString("Move:", DATA_X - 40, MOVE_BAR_Y); // Label
}


// -----------------------------
// 天气初始化任务
// -----------------------------
void Weather_Init_Task(void *pvParameters) {
  drawWeatherStaticElements();
  // Initial time setup (e.g., from RTC or a default value if no RTC)
  // For now, we'll just rely on the system time after boot.
  // If an external RTC is present, its initialization would go here.
  vTaskDelete(NULL);
}



// -----------------------------
// 天气显示任务
// -----------------------------
void Weather_Task(void *pvParameters) {
  for (;;) {
    getLocalTime(&timeinfo); // Get current time
    drawWeatherTime();       // Update time display and progress bars
    vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms for smoother progress bars
  }
}

// 双击检测 (Copied from Buzzer.cpp)
// -----------------------------
// 天气菜单入口
// -----------------------------
void weatherMenu() {
  drawWeatherStaticElements(); // Draw static elements directly
  xTaskCreatePinnedToCore(Weather_Init_Task, "Weather_Init", 8192, NULL, 2, NULL, 0);
  TaskHandle_t weatherTask = NULL;
  xTaskCreatePinnedToCore(Weather_Task, "Weather_Show", 8192, NULL, 1, &weatherTask, 0);
  while (1) {
    if (readButton()) {
      if (detectDoubleClick()) { // 双击返回主菜单
        if (weatherTask != NULL) {
          vTaskDelete(weatherTask);
        }
        break; // Exit weatherMenu
      } else { // 单击：重置喝水和活动计时器
        lastDrinkTime = millis();
        lastMoveTime = millis();
        // Optionally, update display immediately
        drawWeatherTime(); // Redraw to show reset bars
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}