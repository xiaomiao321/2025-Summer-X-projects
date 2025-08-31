#include "MQTT.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "Menu.h" // Include Menu.h to access the menuItems array
#include "Buzzer.h"
#include "System.h" // For TFT logging functions

// --- Configuration ---
// WiFi Configuration
#define WIFI_SSID "xiaomiao_hotspot"
#define WIFI_PASSWORD "xiaomiao123"

// MQTT Configuration
#define MQTT_SERVER "bemfa.com"
#define MQTT_PORT 9501
#define CLIENT_ID "f5ff15f6f91348548bd2038e5d54442e" // Please use your own unique client ID
#define SUB_TOPIC "Light00" // The topic to subscribe to for commands

// --- Global Variables ---
WiFiClient espClient;
PubSubClient client(espClient);
long lastMqttReconnectAttempt = 0;

// Definition of the volatile function pointer
volatile void (*requestedMenuAction)() = nullptr;
volatile bool exitSubMenu = false; // Definition for the exit flag

// --- Forward Declarations ---
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void connectMQTT(); // New function for visual MQTT connection

// --- Core Functions ---

void setupMQTT() {
  // Ensure Serial is initialized if not already
  if (!Serial) {
    Serial.begin(115200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi is connected. Setting up MQTT...");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
    connectMQTT(); // Call the new visual connection function
  } else {
    Serial.println("WiFi not connected, cannot setup MQTT.");
  }
}

void loopMQTT() {
  if (!client.connected()) {
    long now = millis();
    // Try to reconnect every 5 seconds without blocking.
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      reconnect();
    }
  }
} 

// --- Helper Functions ---

void reconnect() {
  Serial.print("Attempting MQTT connection...");
  if (client.connect(CLIENT_ID)) {
    Serial.println("connected");
    client.subscribe(SUB_TOPIC);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.print(" - ");
    // Add detailed error descriptions
    switch (client.state()) {
      case -4:
        Serial.print("MQTT_CONNECTION_TIMEOUT");
        break;
      case -3:
        Serial.print("MQTT_CONNECTION_LOST");
        break;
      case -2:
        Serial.print("MQTT_CONNECT_FAILED");
        break;
      case -1:
        Serial.print("MQTT_DISCONNECTED");
        break;
      case 1:
        Serial.print("MQTT_CONNECT_BAD_PROTOCOL");
        break;
      case 2:
        Serial.print("MQTT_CONNECT_BAD_CLIENT_ID");
        break;
      case 3:
        Serial.print("MQTT_CONNECT_UNAVAILABLE");
        break;
      case 4:
        Serial.print("MQTT_CONNECT_BAD_CREDENTIALS");
        break;
      case 5:
        Serial.print("MQTT_CONNECT_UNAUTHORIZED");
        break;
      default:
        Serial.print("UNKNOWN_ERROR");
        break;
    }
    Serial.println(" try again in 5 seconds");
  }
}

void connectMQTT() {
    tft.fillScreen(TFT_BLACK); // Use TFT_BLACK from TFT_eSPI.h
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM); // Center datum
    tft.drawString("Connecting MQTT...", 120, 20);
    tft_log_y = 40; // Reset log Y position

    char log_buffer[100];
    tftLog("========= MQTT Connect ========", TFT_YELLOW);
    sprintf(log_buffer, "Broker: %s:%d", MQTT_SERVER, MQTT_PORT);
    tftLog(log_buffer, TFT_CYAN);
    sprintf(log_buffer, "Client ID: %s", CLIENT_ID);
    tftLog(log_buffer, TFT_CYAN);

    int attempts = 0;
    int max_attempts = 5;

    while (!client.connected() && attempts < max_attempts) {
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (attempts + 1) * (200 / max_attempts), 13, TFT_GREEN);

        sprintf(log_buffer, "Attempt %d/%d...", attempts + 1, max_attempts);
        tftLog(log_buffer, TFT_YELLOW);

        if (client.connect(CLIENT_ID)) {
            tftLogSuccess("MQTT Connected!");
            client.subscribe(SUB_TOPIC);
            sprintf(log_buffer, "Subscribed to: %s", SUB_TOPIC);
            tftLog(log_buffer, TFT_GREEN);
        } else {
            tftLogError("MQTT Connection Failed!");
            sprintf(log_buffer, "State: %d", client.state());
            tftLog(log_buffer, TFT_RED);
            // Add detailed error descriptions
            switch (client.state()) {
                case -4: tftLogError("MQTT_CONNECTION_TIMEOUT"); break;
                case -3: tftLogError("MQTT_CONNECTION_LOST"); break;
                case -2: tftLogError("MQTT_CONNECT_FAILED"); break;
                case -1: tftLogError("MQTT_DISCONNECTED"); break;
                case 1: tftLogError("MQTT_CONNECT_BAD_PROTOCOL"); break;
                case 2: tftLogError("MQTT_CONNECT_BAD_CLIENT_ID"); break;
                case 3: tftLogError("MQTT_CONNECT_UNAVAILABLE"); break;
                case 4: tftLogError("MQTT_CONNECT_BAD_CREDENTIALS"); break;
                case 5: tftLogError("MQTT_CONNECT_UNAUTHORIZED"); break;
                default: tftLogError("UNKNOWN_ERROR"); break;
            }
            delay(2000); // Give user time to read error
        }
        attempts++;
    }

    if (client.connected()) {
        tftLogSuccess("Setup Complete");
        delay(1500);
    } else {
        tftLogError("Failed to connect MQTT after all attempts.");
        delay(3000);
    }
}
