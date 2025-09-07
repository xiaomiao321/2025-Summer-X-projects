#include "DS18B20.h"
#include "Alarm.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include "MQTT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "weather.h"
// Graph dimensions and position
#define TEMP_GRAPH_WIDTH  200
#define TEMP_GRAPH_HEIGHT 135
#define TEMP_GRAPH_X      20
#define TEMP_GRAPH_Y      90 

// Temperature display position
#define TEMP_VALUE_X      20
#define TEMP_VALUE_Y      20

// Status message position
#define STATUS_MESSAGE_X  10
#define STATUS_MESSAGE_Y  230 // Adjust this to be below the graph

#define DS18B20_PIN 10


OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

float currentTemperature = -127.0;
bool stopDS18B20Task = false;

GraphWidget gr = GraphWidget(&tft);
TraceWidget tr = TraceWidget(&gr);

void DS18B20_Init() {
  sensors.begin();
}

void updateTempTask(void *pvParameters) {
    while(1) {
        sensors.requestTemperatures();
        float temp = sensors.getTempCByIndex(0);
        if (temp != DEVICE_DISCONNECTED_C) {
            currentTemperature = temp;
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Update every 2 seconds
    }
}

void createDS18B20Task() {
    xTaskCreate(
        updateTempTask,
        "DS18B20 Update Task",
        1024,
        NULL,
        1,
        NULL
    );
}

float getDS18B20Temp() {
    return currentTemperature;
}

void DS18B20_Task(void *pvParameters) {
  float lastTemp = -274;
  float gx = 0.0;

  gr.createGraph(TEMP_GRAPH_WIDTH, TEMP_GRAPH_HEIGHT, tft.color565(5, 5, 5));
  gr.setGraphScale(0.0, 100.0, 0.0, 40.0); // X-axis 0-100, Y-axis 0-40 (temperature range)
  gr.setGraphGrid(0.0, 25.0, 0.0, 10.0, TFT_DARKGREY); // Grid every 25 on X, 10 on Y
  gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
  tr.startTrace(TFT_YELLOW);

  // Draw Y-axis labels
  tft.setTextSize(1); // Set text size for axis labels
  tft.setTextDatum(MR_DATUM); // Middle-Right datum
  tft.setTextColor(TFT_WHITE, tft.color565(5, 5, 5)); // Set text color for axis labels
  tft.drawNumber(40, gr.getPointX(0.0) - 5, gr.getPointY(40.0));
  tft.drawNumber(30, gr.getPointX(0.0) - 5, gr.getPointY(30.0));
  tft.drawNumber(20, gr.getPointX(0.0) - 5, gr.getPointY(20.0));
  tft.drawNumber(10, gr.getPointX(0.0) - 5, gr.getPointY(10.0));
  tft.drawNumber(0, gr.getPointX(0.0) - 5, gr.getPointY(0.0));

  // Draw X-axis labels
  tft.setTextDatum(TC_DATUM); // Top-Center datum
  tft.drawNumber(0, gr.getPointX(0.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(25, gr.getPointX(25.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(50, gr.getPointX(50.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(75, gr.getPointX(75.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(100, gr.getPointX(100.0), gr.getPointY(0.0) + 5);

  while (1) {
    if (stopDS18B20Task) {
      break;
    }

    float tempC = getDS18B20Temp();

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      tft.fillRect(0, 0, tft.width(), TEMP_GRAPH_Y - 5, TFT_BLACK); // Clear area above graph
      
      // Display current time at the top
      if (!getLocalTime(&timeinfo)) {
          // Handle error or display placeholder
      } else {
          char time_str[30]; // Increased buffer size
          strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
          tft.setTextFont(2); // Smaller font for time
          tft.setTextSize(1);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setTextDatum(MC_DATUM); // Center align
          tft.drawString(time_str, tft.width() / 2, 10); // Position at top center
          tft.setTextDatum(TL_DATUM); // Reset datum
      }

      char tempStr[10];
      dtostrf(tempC, 4, 2, tempStr);
      
      tft.setTextFont(7); // Set font to 7
      tft.setTextSize(1); // Set size to 1

      // Calculate position to center the text
      // Need to combine tempStr and " C" for accurate width calculation
      char fullTempStr[15];
      sprintf(fullTempStr, "%s C", tempStr);

      int text_width = tft.textWidth(fullTempStr);
      int text_height = tft.fontHeight();
      int x_pos = (tft.width() - text_width) / 2;
      // Adjust y_pos to account for the time display at the top
      int y_pos = (TEMP_GRAPH_Y - 5 - text_height) / 2 + 20; // Shift down by ~20 pixels for time

      tft.setTextDatum(TL_DATUM); // Set to Top-Left for precise positioning
      tft.setTextColor(TFT_WHITE, TFT_BLACK); // Ensure text color is white on black background
      tft.drawString(fullTempStr, x_pos, y_pos);

      tr.addPoint(gx, tempC - 0.5);
      tr.addPoint(gx, tempC);
      tr.addPoint(gx, tempC + 0.5);
      gx += 1.0;

      if (gx > 100.0) {
        gx = 0.0;
        gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
        tr.startTrace(TFT_YELLOW);
      }

      lastTemp = tempC;
    } else {
      if (stopDS18B20Task) break;

      tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK); // Clear entire screen on error
      tft.setCursor(10, 30);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("Sensor Error!");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  vTaskDelete(NULL);
}

void DS18B20Menu() {
  stopDS18B20Task = false;

  tft.fillScreen(TFT_BLACK);

  xTaskCreatePinnedToCore(DS18B20_Task, "DS18B20_Task", 4096, NULL, 1, NULL, 0);

  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        stopDS18B20Task = true;
        vTaskDelay(pdMS_TO_TICKS(150)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        stopDS18B20Task = true; // Signal task to stop
        vTaskDelay(pdMS_TO_TICKS(150)); // Give task time to stop
        break; // Exit loop
    }
    if (readButton()) {
      stopDS18B20Task = true;
      TaskHandle_t task = xTaskGetHandle("DS18B20_Task");
      for (int i = 0; i < 150 && task != NULL; i++) {
        if (eTaskGetState(task) == eDeleted) break;
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
