#include "WiFiManager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <AsyncUDP.h>
#include <Preferences.h>
#include "Menu.h" // For using the menuSprite for display
#include "System.h" // For tftLog
#include "weather.h" // For connectWiFi() fallback

// --- Objects ---
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// --- Variables ---
const char* ap_ssid = "WeatherClock_Setup";

// =====================================================================================
//                                     HTML PAGE
// =====================================================================================

const char* wifi_manager_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #282c34; color: #fff; text-align: center; }
        .container { max-width: 400px; margin: 0 auto; padding: 20px; }
        h1 { color: #61afef; }
        select, input, button { width: 100%; padding: 12px; margin: 10px 0; border-radius: 5px; border: 1px solid #61afef; background-color: #3c4049; color: #fff; box-sizing: border-box; }
        button { background-color: #61afef; cursor: pointer; font-weight: bold; }
        .wifi-list { text-align: left; }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Configuration</h1>
        <form action="/save" method="POST">
            <div class="wifi-list">
                <label for="ssid">Select a Network:</label>
                <select id="ssid_select" onchange="document.getElementById('ssid_input').value = this.value;">
                    <option value="">-- Scan Results --</option>
                    %WIFI_LIST%
                </select>
                <label for="ssid_input">Or Enter SSID:</label>
                <input type="text" id="ssid_input" name="ssid" placeholder="Network Name">
            </div>
            <label for="password">Password:</label>
            <input type="password" name="password" placeholder="Password">
            <button type="submit">Save and Connect</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

// =====================================================================================
//                                 WEB SERVER HANDLERS
// =====================================================================================

void handleRoot() {
    // Scan for WiFi networks
    String wifi_list_options = "";
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        wifi_list_options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ")</option>";
    }

    String html = wifi_manager_html;
    html.replace("%WIFI_LIST%", wifi_list_options);
    server.send(200, "text/html", html);
}

void handleSave() {
    tftClearLog();
    tftLog("Web page submitted.", TFT_WHITE);

    String ssid = server.arg("ssid");
    String password = server.arg("password");

    Serial.println("Received new WiFi credentials:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + password);

    // Save credentials to Preferences
    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();

    // Display saving message
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.drawString("Credentials Saved!", 120, 80);
    menuSprite.drawString("Rebooting...", 120, 120);
    menuSprite.pushSprite(0,0);

    server.send(200, "text/plain", "Credentials saved. The device will now reboot and try to connect.");
    delay(2000);
    ESP.restart();
}

void handleNotFound(){
    server.send(302, "text/plain", "http://192.168.4.1"); // Redirect to the root page
}

// =====================================================================================
//                                     MAIN FUNCTION
// =====================================================================================

bool connectWiFi_with_Manager() {
    // If already connected, do nothing.
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    tft.fillScreen(TFT_BLACK);
    tftClearLog();

    preferences.begin("wifi-creds", true); // Read-only mode first
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("password", "");
    preferences.end();

    // --- Try to connect with saved credentials ---
    if (saved_ssid.length() > 0) {
        tftLog("Found saved network:", TFT_WHITE);
        tftLog(saved_ssid.c_str(), TFT_CYAN);
        tftLog("Connecting...", TFT_WHITE);

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
        tftLog("WiFi.begin() called", TFT_YELLOW);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            tftLog("attempt " + String(attempts+1) + " of 10", TFT_WHITE); // 5 second timeout
            delay(500);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            tftLog("Connection SUCCESS!", TFT_GREEN);
            delay(1500);
            return true; // Success
        }
        
        tftLog("Connection FAILED.", TFT_RED);
        tftLog("Trying fallback method...", TFT_YELLOW);
        delay(2000);

        // Call the fallback function from weather.cpp
        if (connectWiFi()) {
            // Fallback connection was successful
            tftClearLog();
            tftLog("Fallback SUCCESS!", TFT_GREEN);
            delay(1500);
            return true;
        }
        tftClearLog();
        // If we are here, both saved credentials and fallback failed.
        tftLog("Fallback FAILED.", TFT_RED);
        delay(2000);

    } else {
        tftLog("No saved network found.", TFT_YELLOW);
        delay(1500);
    }

    // --- If connection fails, start AP mode and Config Portal ---
    tftClearLog();
    tftLog("Starting Config Portal...", TFT_YELLOW);
    tftLog("Please connect to AP:", TFT_WHITE);
    tftLog(ap_ssid, TFT_CYAN);
    tftLog("Then open browser to:", TFT_WHITE);
    tftLog("192.168.4.1", TFT_CYAN);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid);
    IPAddress myIP = WiFi.softAPIP();
    Serial.println("AP IP address: " + myIP.toString());

    // Start DNS and Web server
    dnsServer.start(53, "*", myIP);
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot); // Captive portal
    server.begin();

    Serial.println("Config portal running.");

    // Loop to handle web requests
    while(true) {
        dnsServer.processNextRequest();
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return false; // Should not be reached
}