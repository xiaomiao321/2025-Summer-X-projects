#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "display.h"
#include "sensors.h"
#include "pcdata.h"
#include "network.h"
#include "encoder.h"

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Create mutex
  xPCDataMutex = xSemaphoreCreateMutex();

  // Initialize encoder
  initEncoder();

  // Start initialization tasks
  xTaskCreatePinnedToCore(AHT_Init_Task, "AHT_Init", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TFT_Init_Task, "TFT_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(WIFI_Init_Task, "WiFi_Init", 8192, NULL, 2, NULL, 0);

  // Start running tasks
  xTaskCreatePinnedToCore(AHT_Task, "AHT_Read", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TFT_Task, "TFT_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(Encoder_Task, "Encoder", 2048, NULL, 1, NULL, 0);
}

void loop() {
  delay(1000);
}