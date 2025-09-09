#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

// This volatile function pointer will be set by the MQTT callback
// and checked by the main loop in the .ino file.
extern volatile void (*requestedMenuAction)();
extern volatile bool exitSubMenu; // Flag to exit a submenu

// Initializes WiFi and MQTT connection
void reconnect();
void connectMQTT(); // New function for visual MQTT connection

// Keeps the MQTT client running, should be called in the main loop
void loopMQTT();

#endif
