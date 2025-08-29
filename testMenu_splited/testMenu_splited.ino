#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Menu.h"
#include "System.h"


void setup() {
    bootSystem();
}

void loop() {
    showMenu();
    vTaskDelay(pdMS_TO_TICKS(15));
}