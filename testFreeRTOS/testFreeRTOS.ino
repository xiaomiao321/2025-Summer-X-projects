// 不要 include "Arduino_FreeRTOS.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define LED_PIN 8

// Declare task handle
TaskHandle_t BlinkTaskHandle = NULL;

void BlinkTask(void *parameter) {
  for (;;) { // Infinite loop
    digitalWrite(LED_PIN, HIGH);
    Serial.println("BlinkTask: LED ON");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000ms
    digitalWrite(LED_PIN, LOW);
    Serial.println("BlinkTask: LED OFF");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("BlinkTask running on core ");
    Serial.println(xPortGetCoreID());
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);

  xTaskCreatePinnedToCore(
    BlinkTask,         // Task function
    "BlinkTask",       // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameters
    1,                 // Priority
    &BlinkTaskHandle,  // Task handle
    0                  // Core 0
  );
}

void loop() {
  // Empty because FreeRTOS scheduler runs the task
}