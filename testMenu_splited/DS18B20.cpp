#include "DS18B20.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>

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

  gr.createGraph(100, 50, tft.color565(5, 5, 5));
  gr.setGraphScale(0.0, 100.0, 0.0, 40.0);
  gr.setGraphGrid(0.0, 50.0, 0.0, 20.0, TFT_DARKGREY);
  gr.drawGraph(14, 75);
  tr.startTrace(TFT_YELLOW);

  tft.setTextDatum(MR_DATUM);
  tft.drawNumber(40, gr.getPointX(0.0) - 2, gr.getPointY(40.0));
  tft.drawNumber(20, gr.getPointX(0.0) - 2, gr.getPointY(20.0));
  tft.drawNumber(0, gr.getPointX(0.0) - 2, gr.getPointY(0.0));

  tft.setTextDatum(TC_DATUM);
  tft.drawNumber(0, gr.getPointX(0.0), gr.getPointY(0.0) + 2);
  tft.drawNumber(50, gr.getPointX(50.0), gr.getPointY(0.0) + 2);
  tft.drawNumber(100, gr.getPointX(100.0), gr.getPointY(0.0) + 2);

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
      tft.fillRect(0, 0, tft.width(), 60, TFT_BLACK);
      char tempStr[10];
      dtostrf(tempC, 4, 2, tempStr);
      tft.setTextSize(4);
      tft.setCursor(20, 20);
      tft.print(tempStr);
      tft.print(" C");

      tr.addPoint(gx, tempC - 0.5);
      tr.addPoint(gx, tempC);
      tr.addPoint(gx, tempC + 0.5);
      gx += 1.0;

      if (gx > 100.0) {
        gx = 0.0;
        gr.drawGraph(14, 75);
        tr.startTrace(TFT_YELLOW);
      }

      tft.fillRect(0, 130, tft.width(), 20, TFT_BLACK);
      tft.setTextSize(2);
      if (tempC > 35) {
        tft.setTextColor(TFT_RED);
        tft.setCursor(10, 130);
        tft.println("Too High");
      } else if (tempC < 10) {
        tft.setTextColor(TFT_BLUE);
        tft.setCursor(10, 130);
        tft.println("Too Low");
      } else {
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 130);
        tft.println("Normal");
      }
      lastTemp = tempC;
    } else {
      if (stopDS18B20Task) break;

      tft.fillRect(0, 0, tft.width(), 128, TFT_BLACK);
      tft.setCursor(10, 30);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("Sensor Error!");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

exit_task:
  vTaskDelete(NULL);
}

void DS18B20Menu() {
  stopDS18B20Task = false;

  animateMenuTransition("DS18B20", true);

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

  animateMenuTransition("DS18B20", false);
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}