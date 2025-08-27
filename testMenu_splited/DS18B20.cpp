#include "DS18B20.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>

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

bool stopDS18B20Task = false;

GraphWidget gr = GraphWidget(&tft);
TraceWidget tr = TraceWidget(&gr);

void DS18B20_Init_Task(void *pvParameters) {
  sensors.begin();

  DeviceAddress tempAddress;
  delay(1000);
  vTaskDelete(NULL);
}

void DS18B20_Task(void *pvParameters) {
  float lastTemp = -274;
  unsigned long timeout;
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

    sensors.requestTemperatures();

    timeout = millis() + 750;
    while (millis() < timeout) {
      if (stopDS18B20Task) {
        goto exit_task;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    float tempC = sensors.getTempCByIndex(0);

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      tft.fillRect(0, 0, tft.width(), TEMP_GRAPH_Y - 5, TFT_BLACK); // Clear area above graph
      char tempStr[10];
      dtostrf(tempC, 4, 2, tempStr);
      tft.setTextSize(4);
      tft.setCursor(TEMP_VALUE_X, TEMP_VALUE_Y);
      tft.print(tempStr);
      tft.print(" C");

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

exit_task:
  vTaskDelete(NULL);
}

void DS18B20Menu() {
  stopDS18B20Task = false;

  tft.fillScreen(TFT_BLACK);

  xTaskCreatePinnedToCore(DS18B20_Init_Task, "DS18B20_Init", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(DS18B20_Task, "DS18B20_Task", 4096, NULL, 1, NULL, 0);

  while (1) {
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

  // display = 48;
  // picture_flag = 0;
  // showMenuConfig();
}