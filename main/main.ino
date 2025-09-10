#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LittleFS.h>

#include "Menu.h"
#include "System.h"
#include "MQTT.h" // Include the MQTT header
#include "weather.h" // For the advanced connectWiFi() function
#include <random>
#include "DS18B20.h"
#include "WiFiManager.h"
#include "Alarm.h"
#include "TargetSettings.h"



void setup() {
    bootSystem();

    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");
    
    // Initialize MQTT, assuming WiFi is now connected
    // connectMQTT(); 
}

void loop() {
    // Alarm_Loop_Check(); // Now runs in a background task
    // loopMQTT(); // Keep the MQTT client running

    // // Check if a menu navigation was requested via MQTT
    // if (requestedMenuAction != nullptr) {
    //     // A menu was requested via MQTT
    //     void (*actionToRun)() = (void (*)())requestedMenuAction; // Copy to a local, non-volatile variable
    //     requestedMenuAction = nullptr; // Reset the flag immediately
        
    //     actionToRun();      // Execute the requested menu function
    //     showMenuConfig();   // Redraw the main menu after the submenu exits
    // } else {
    //     // No MQTT command, proceed with normal encoder-based menu
    //     showMenu();
    // }
    showMenu();
    vTaskDelay(pdMS_TO_TICKS(15));
}