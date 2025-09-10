#include "Internet.h"
#include "System.h" // For tftLog functions
#include "RotaryEncoder.h" // For readEncoder, readButton, readButtonLongPress
#include "Menu.h" // For showMenuConfig and other menu related functions
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "font_12.h"
// Global display objects (defined in main.ino or Menu.cpp)
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
// Define the API key
const char* INTERNET_API_KEY = "d8c6d4c75ba0";

// API URLs
// const char* HEALTH_TIP_API_URL = "https://whyta.cn/api/tx/healthtip?key=";
// const char* SAY_LOVE_API_URL = "https://whyta.cn/api/tx/saylove?key=";
// const char* EVERYDAY_ENGLISH_API_URL = "https://whyta.cn/api/tx/everyday?key=";
// const char* FORTUNE_API_URL = "https://whyta.cn/api/fortune?key=";
// const char* SHICI_API_URL = "https://whyta.cn/api/shici?key=";
// const char* DUILIAN_API_URL = "https://whyta.cn/api/tx/duilian?key=";
// const char* FX_RATE_API_URL = "https://whyta.cn/api/tx/fxrate?key=";
// const char* RANDOM_EN_WORD_API_URL = "https://ilovestudent.cn/api/commonApi/randomEnWord"; // No key needed
const char* HEALTH_TIP_API_URL = "http://whyta.cn/api/tx/healthtip?key=";
const char* SAY_LOVE_API_URL = "http://whyta.cn/api/tx/saylove?key=";
const char* EVERYDAY_ENGLISH_API_URL = "http://whyta.cn/api/tx/everyday?key=";
const char* FORTUNE_API_URL = "http://whyta.cn/api/fortune?key=";
const char* SHICI_API_URL = "http://whyta.cn/api/shici?key=";
const char* DUILIAN_API_URL = "http://whyta.cn/api/tx/duilian?key=";
const char* FX_RATE_API_URL = "http://whyta.cn/api/tx/fxrate?key=";
const char* RANDOM_EN_WORD_API_URL = "http://ilovestudent.cn/api/commonApi/randomEnWord"; // No key needed
// Global data storage for Internet menu
HealthTipData g_health_tip_data;
SayLoveData g_say_love_data;
EverydayEnglishData g_everyday_english_data;
FortuneData g_fortune_data;
ShiciData g_shici_data;
DuilianData g_duilian_data;
FxRateData g_fx_rate_data;
RandomEnWordData g_random_en_word_data;

// Internal variables for page management
static int g_current_internet_page = 0;
static const int MAX_INTERNET_PAGES = 8; // Total number of API pages

// --- Helper function for HTTP GET requests ---
String httpGETRequest(const char* url, int maxRetries = 5) {
    if (WiFi.status() != WL_CONNECTED) {
        tftLog("HTTP: No WiFi", TFT_RED);
        Serial.println("HTTP GET Failed: WiFi not connected");
        return "N/A";
    }

    tftLog("HTTP: Start...", TFT_YELLOW);
    Serial.printf("HTTP GET: %s\n", url);

    WiFiClient client;
    HTTPClient http;
    String payload = "N/A";
    bool success = false;

    char log_buffer[100];

    // 显示重试进度条（假设 tft.height() = 240）
    tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
    
    for (int i = 0; i < maxRetries; i++) {
        // 填充进度条
        tft.fillRect(21, tft.height() - 18, (i + 1) * (200 / maxRetries), 13, TFT_BLUE);

        sprintf(log_buffer, "HTTP Try %d/%d", i + 1, maxRetries);
        tftLog(log_buffer, TFT_YELLOW);

        // 检查是否是 HTTP URL
        if (strncmp(url, "http://", 7) != 0) {
            sprintf(log_buffer, "URL invalid: %s", url);
            tftLog(log_buffer, TFT_RED);
            delay(500);
            continue;
        }

        // 清理 client 和 http 状态
        client.stop();
        http.end();

        delay(100); // 短暂延迟，避免频繁连接

        if (http.begin(client, url)) {
            http.setTimeout(5000); // 5秒超时
            http.addHeader("User-Agent", "ESP32-HTTP-Client/1.0");
            http.addHeader("Connection", "close"); // 避免 keep-alive 导致资源占用

            sprintf(log_buffer, "Requesting...");
            tftLog(log_buffer, TFT_YELLOW);

            int httpCode = http.GET();

            if (httpCode > 0) {
                sprintf(log_buffer, "HTTP: %d", httpCode);
                tftLog(log_buffer, (httpCode == HTTP_CODE_OK) ? TFT_GREEN : TFT_RED);

                if (httpCode == HTTP_CODE_OK) {
                    payload = http.getString();
                    tftLog("Success!", TFT_GREEN);
                    success = true;
                    http.end();
                    break;
                } else {
                    sprintf(log_buffer, "Error: %s", http.errorToString(httpCode).c_str());
                    tftLog(log_buffer, TFT_RED);
                }
            } else {
                sprintf(log_buffer, "Failed: %s", http.errorToString(httpCode).c_str());
                tftLog(log_buffer, TFT_RED);
            }

            http.end(); // 释放资源
        } else {
            tftLog("Begin failed", TFT_RED);
        }

        delay(1000); // 每次重试间隔 1 秒
    }

    // 重试结束后，如果成功，填充完整进度条
    if (success) {
        tft.fillRect(21, tft.height() - 18, 200, 13, TFT_GREEN);
    } else {
        tftLog("HTTP FAILED", TFT_RED);
    }

    return success ? payload : "N/A";
}

// --- Individual API Fetch Functions ---
void fetchHealthTip() {
    String url = String(HEALTH_TIP_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_health_tip_data.content = doc["result"]["content"].as<String>();
            tftLogInfo("Health Tip fetched.");
            Serial.println("Health Tip: " + g_health_tip_data.content);
        } else {
            tftLogError("Health Tip JSON error: " + String(error.c_str()));
        }
    }
}

void fetchSayLove() {
    String url = String(SAY_LOVE_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_say_love_data.content = doc["result"]["content"].as<String>();
            tftLogInfo("Say Love fetched.");
            Serial.println("Say Love: " + g_say_love_data.content);
        } else {
            tftLogError("Say Love JSON error: " + String(error.c_str()));
        }
    }
}

void fetchEverydayEnglish() {
    String url = String(EVERYDAY_ENGLISH_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_everyday_english_data.content = doc["result"]["content"].as<String>();
            g_everyday_english_data.note = doc["result"]["note"].as<String>();
            tftLogInfo("Everyday English fetched.");
            Serial.println("Everyday English: " + g_everyday_english_data.content + " (" + g_everyday_english_data.note + ")");
        } else {
            tftLogError("Everyday English JSON error: " + String(error.c_str()));
        }
    }
}

void fetchFortune() {
    String url = String(FORTUNE_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_fortune_data.success = doc["success"].as<bool>();
            if (g_fortune_data.success) {
                g_fortune_data.sign = doc["data"]["sign"].as<String>();
                g_fortune_data.description = doc["data"]["description"].as<String>();
                g_fortune_data.luckyColor = doc["data"]["luckyColor"].as<String>();
                g_fortune_data.luckyNumber = doc["data"]["luckyNumber"].as<int>();
                g_fortune_data.message = ""; // Clear any previous message
                Serial.println("Fortune: " + g_fortune_data.sign + ", " + g_fortune_data.description + ", Color: " + g_fortune_data.luckyColor + ", Number: " + String(g_fortune_data.luckyNumber));
            } else {
                g_fortune_data.message = doc["message"].as<String>();
                Serial.println("Fortune Error: " + g_fortune_data.message);
            }
            tftLogInfo("Fortune fetched.");
        } else {
            tftLogError("Fortune JSON error: " + String(error.c_str()));
        }
    }
}

void fetchShici() {
    String url = String(SHICI_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_shici_data.content = doc["data"]["content"].as<String>();
            g_shici_data.author = doc["data"]["origin"]["author"].as<String>();
            g_shici_data.dynasty = doc["data"]["origin"]["dynasty"].as<String>();
            tftLogInfo("Shici fetched.");
            Serial.println("Shici: " + g_shici_data.content + " --" + g_shici_data.author + " [" + g_shici_data.dynasty + "]");
        } else {
            tftLogError("Shici JSON error: " + String(error.c_str()));
        }
    }
}

void fetchDuilian() {
    String url = String(DUILIAN_API_URL) + INTERNET_API_KEY;
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_duilian_data.content = doc["result"]["content"].as<String>();
            tftLogInfo("Duilian fetched.");
            Serial.println("Duilian: " + g_duilian_data.content);
        } else {
            tftLogError("Duilian JSON error: " + String(error.c_str()));
        }
    }
}

void fetchFxRate() {
    String url = String(FX_RATE_API_URL) + INTERNET_API_KEY + "&fromcoin=USD&tocoin=CNY&money=1";
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_fx_rate_data.money = doc["result"]["money"].as<String>();
            tftLogInfo("FxRate fetched.");
            Serial.println("FxRate (1 USD to CNY): " + g_fx_rate_data.money);
        } else {
            tftLogError("FxRate JSON error: " + String(error.c_str()));
        }
    }
}

void fetchRandomEnWord() {
    String url = String(RANDOM_EN_WORD_API_URL);
    String payload = httpGETRequest(url.c_str());
    if (payload.length() > 0) {
        DynamicJsonDocument doc(2048); // Increased capacity for potentially larger word data
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            g_random_en_word_data.headWord = doc["data"]["headWord"].as<String>();
            g_random_en_word_data.tranCn = doc["data"]["tranCn"].as<String>();
            
            // Concatenate phrases
            g_random_en_word_data.phrases_en = "";
            g_random_en_word_data.phrases_cn = "";
            JsonArray phrases = doc["data"]["phrases"].as<JsonArray>();
            for (JsonObject phrase : phrases) {
                if (g_random_en_word_data.phrases_en.length() > 0) g_random_en_word_data.phrases_en += "\n";
                if (g_random_en_word_data.phrases_cn.length() > 0) g_random_en_word_data.phrases_cn += "\n";
                g_random_en_word_data.phrases_en += phrase["content"].as<String>();
                g_random_en_word_data.phrases_cn += phrase["cn"].as<String>();
            }
            tftLogInfo("Random English Word fetched.");
            Serial.println("Random Word: " + g_random_en_word_data.headWord + " - " + g_random_en_word_data.tranCn);
            if (g_random_en_word_data.phrases_en.length() > 0) {
                Serial.println("Phrases:\n" + g_random_en_word_data.phrases_en + "\n" + g_random_en_word_data.phrases_cn);
            }
        } else {
            tftLogError("Random En Word JSON error: " + String(error.c_str()));
        }
    }
}

// --- Internet Menu Core Functions ---

void internet_menu_init() {
    tftLogInfo("Initializing Internet Menu...");
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("Connecting WiFi...", tft.width() / 2, tft.height() / 2 - 10);

    // Explicitly wait for WiFi to connect, similar to the test program
    int dot_x = tft.width() / 2 + tft.textWidth("Connecting WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        tft.drawString(".", dot_x, tft.height() / 2 - 10);
        dot_x += tft.textWidth(".");
        if (dot_x > tft.width() - 10) { // Reset dots if they go off screen
            tft.fillRect(tft.width() / 2 + tft.textWidth("Connecting WiFi..."), tft.height() / 2 - 10, tft.width()/2, tft.fontHeight(), TFT_BLACK);
            dot_x = tft.width() / 2 + tft.textWidth("Connecting WiFi...");
        }
        // tftLogDebug("Waiting for WiFi..."); // Removed log
    }
    tft.fillScreen(TFT_BLACK); // Clear screen after connection
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("WiFi Connected!", tft.width() / 2, tft.height() / 2);
    delay(1000);

    menuSprite.createSprite(tft.width(), tft.height()); // Initialize menuSprite for double buffering
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.drawString("Loading Data...", tft.width() / 2, tft.height() / 2);
    menuSprite.pushSprite(0, 0); // Show loading message


    // Fetch all data initially
    // fetchHealthTip();

    fetchSayLove();

    fetchEverydayEnglish();

    fetchFortune();

    fetchShici();

    fetchDuilian();

    fetchFxRate();

    fetchRandomEnWord();

    tftLogInfo("Internet Menu Initialized.");
    g_current_internet_page = 0; // Start at the first page
    internet_menu_draw(); // Draw the first page
}

void internet_menu_loop() {
    // This function is called repeatedly while the Internet menu is active
    int rotation = readEncoder();
    bool button_clicked = readButton();
    bool long_press = readButtonLongPress();

    if (rotation != 0) {
        if (rotation > 0) {
            internet_menu_next_page();
        } else {
            internet_menu_prev_page();
        }
        internet_menu_draw(); // Redraw on page change
    }

    if (button_clicked) {
        // Re-fetch data for the current page
        tftClearLog();
        tftLogInfo("Re-fetching data for current page...");
        switch (g_current_internet_page) {
            case 0: fetchHealthTip(); break;
            case 1: fetchSayLove(); break;
            case 2: fetchEverydayEnglish(); break;
            case 3: fetchFortune(); break;
            case 4: fetchShici(); break;
            case 5: fetchDuilian(); break;
            case 6: fetchFxRate(); break;
            case 7: fetchRandomEnWord(); break;
        }
        internet_menu_draw(); // Redraw with new data
    }

    if (long_press) {
        internet_menu_back_press();
        // The calling loop in Menu.cpp will break out after this
    }
}

// Helper function to draw wrapped text on a sprite
void drawWrappedText(TFT_eSprite &sprite, String text, int x, int y, int maxWidth, int font, uint16_t color) {
    sprite.setTextColor(color);
    sprite.setTextFont(font);
    sprite.setTextWrap(true); // Enable text wrapping
    sprite.setCursor(x, y);   // Set the starting cursor position
    sprite.print(text);       // Print the text, it will wrap automatically
}

void internet_menu_draw() {
    menuSprite.fillScreen(TFT_BLACK); // Draw to sprite
    menuSprite.setTextDatum(TL_DATUM); // Top-Left datum for general text
    menuSprite.setTextColor(TFT_WHITE);

    // Page indicator - English
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(2);
    menuSprite.setTextDatum(TR_DATUM);
    menuSprite.setTextColor(TFT_LIGHTGREY);
    menuSprite.drawString(String(g_current_internet_page + 1) + "/" + String(MAX_INTERNET_PAGES), menuSprite.width() - 5, 5);
    menuSprite.setTextDatum(TL_DATUM); // Reset to TL_DATUM

    int content_x = 5;
    int content_y = 45;
    int content_width = menuSprite.width() - 10; // 5 pixels padding on each side
    int english_font_num = 1; // For English, using built-in font 1

    // Draw content based on current page
    switch (g_current_internet_page) {
        case 0: // Health Tip
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_CYAN);
            menuSprite.drawString("Health Tip:", 5, 25);
            
            // Content (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, g_health_tip_data.content, content_x, content_y, content_width, 0, TFT_WHITE);
            menuSprite.unloadFont();
            break;
        case 1: // Say Love
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_PINK);
            menuSprite.drawString("Love Talk:", 5, 25);

            // Content (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, g_say_love_data.content, content_x, content_y, content_width, 0, TFT_WHITE);
            menuSprite.unloadFont();
            break;
        case 2: // Everyday English
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_GREEN);
            menuSprite.drawString("Daily English:", 5, 25);

            // Content (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            drawWrappedText(menuSprite, g_everyday_english_data.content, content_x, content_y, content_width, english_font_num, TFT_WHITE);
            content_y = menuSprite.getCursorY() + 10;

            // Note (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, g_everyday_english_data.note, content_x, content_y, content_width, 0, TFT_YELLOW);
            menuSprite.unloadFont();
            break;
        case 3: // Fortune
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_ORANGE);
            menuSprite.drawString("Daily Fortune:", 5, 25);

            if (g_fortune_data.success) {
                menuSprite.loadFont(font_12);
                drawWrappedText(menuSprite, "Sign: " + g_fortune_data.sign, content_x, content_y, content_width, 0, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 5;
                drawWrappedText(menuSprite, "Desc: " + g_fortune_data.description, content_x, content_y, content_width, 0, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 5;
                drawWrappedText(menuSprite, "Color: " + g_fortune_data.luckyColor, content_x, content_y, content_width, 0, TFT_WHITE);
                menuSprite.unloadFont();
                
                content_y = menuSprite.getCursorY() + 5;
                
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                drawWrappedText(menuSprite, "Num: " + String(g_fortune_data.luckyNumber), content_x, content_y, content_width, english_font_num, TFT_WHITE);
            } else {
                menuSprite.loadFont(font_12);
                drawWrappedText(menuSprite, "Error: " + g_fortune_data.message, content_x, content_y, content_width, 0, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 5;
                drawWrappedText(menuSprite, "Sign: " + g_fortune_data.sign, content_x, content_y, content_width, 0, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 5;
                drawWrappedText(menuSprite, "Desc: " + g_fortune_data.description, content_x, content_y, content_width, 0, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 5;
                drawWrappedText(menuSprite, "Color: " + g_fortune_data.luckyColor, content_x, content_y, content_width, 0, TFT_WHITE);
                menuSprite.unloadFont();

                content_y = menuSprite.getCursorY() + 5;
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                drawWrappedText(menuSprite, "Num: " + String(g_fortune_data.luckyNumber), content_x, content_y, content_width, english_font_num, TFT_WHITE);
            }
            break;
        case 4: // Shici (Poem)
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_MAGENTA);
            menuSprite.drawString("Daily Poem:", 5, 25);

            // Content (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, g_shici_data.content, content_x, content_y, content_width, 0, TFT_WHITE);
            content_y = menuSprite.getCursorY() + 10;
            drawWrappedText(menuSprite, "Author: " + g_shici_data.author, content_x, content_y, content_width, 0, TFT_YELLOW);
            content_y = menuSprite.getCursorY() + 10;
            drawWrappedText(menuSprite, "Dynasty: " + g_shici_data.dynasty, content_x, content_y, content_width, 0, TFT_YELLOW);
            menuSprite.unloadFont();
            break;
        case 5: // Duilian (Couplet)
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_BLUE);
            menuSprite.drawString("Couplet:", 5, 25);

            // Content (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, g_duilian_data.content, content_x, content_y, content_width, 0, TFT_WHITE);
            menuSprite.unloadFont();
            break;
        case 6: // FxRate (Exchange Rate)
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_GREENYELLOW);
            menuSprite.drawString("USD to CNY (1 USD):", 5, 25);

            // Content (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            drawWrappedText(menuSprite, g_fx_rate_data.money + " CNY", content_x, content_y, content_width, english_font_num, TFT_WHITE);
            break;
        case 7: // Random English Word
            // Title (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            menuSprite.setTextColor(TFT_VIOLET);
            menuSprite.drawString("Random Word:", 5, 25);

            // Word (English)
            menuSprite.setTextFont(1);
            menuSprite.setTextSize(2);
            drawWrappedText(menuSprite, "Word: " + g_random_en_word_data.headWord, content_x, content_y, content_width, english_font_num, TFT_WHITE);
            content_y = menuSprite.getCursorY() + 10;

            // Trans (Chinese)
            menuSprite.loadFont(font_12);
            drawWrappedText(menuSprite, "Trans: " + g_random_en_word_data.tranCn, content_x, content_y, content_width, 0, TFT_WHITE);
            menuSprite.unloadFont();

            if (g_random_en_word_data.phrases_en.length() > 0) {
                content_y = menuSprite.getCursorY() + 10;
                
                // Phrases title (English)
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                menuSprite.setTextColor(TFT_CYAN);
                menuSprite.drawString("Phrases:", content_x, content_y);
                content_y = menuSprite.getCursorY() + 10;

                // English phrases
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                drawWrappedText(menuSprite, g_random_en_word_data.phrases_en, content_x, content_y, content_width, english_font_num, TFT_WHITE);
                content_y = menuSprite.getCursorY() + 10;

                // Chinese phrases
                menuSprite.loadFont(font_12);
                drawWrappedText(menuSprite, g_random_en_word_data.phrases_cn, content_x, content_y, content_width, 0, TFT_WHITE);
                menuSprite.unloadFont();
            }
            break;
    }
    menuSprite.pushSprite(0, 0); // Push sprite to screen
}

void internet_menu_next_page() {
    g_current_internet_page = (g_current_internet_page + 1) % MAX_INTERNET_PAGES;
    // tftLogInfo("Internet Page: " + String(g_current_internet_page + 1));
}

void internet_menu_prev_page() {
    g_current_internet_page = (g_current_internet_page - 1 + MAX_INTERNET_PAGES) % MAX_INTERNET_PAGES;
    // tftLogInfo("Internet Page: " + String(g_current_internet_page + 1));
}

bool internet_menu_back_press() {
    return true; // Indicate that the menu should be exited
}

// New function to encapsulate internet_menu_init and the main loop
void InternetMenuScreen() {
    internet_menu_init(); // Initialize the Internet menu
    while (1) { // internet_menu_back_press() will be true on long press
        internet_menu_loop();
        internet_menu_draw();
        if (readButtonLongPress()) { return; }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
    // No need to call showMenuConfig() here, Menu.cpp will handle returning to main menu
}
