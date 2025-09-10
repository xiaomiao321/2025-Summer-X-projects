#ifndef INTERNET_H
#define INTERNET_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "RotaryEncoder.h" // For encoder functions
#include "System.h" // For tftLog functions



// Structs to hold parsed data from each API
struct HealthTipData {
    String content;
};

struct SayLoveData {
    String content;
};

struct EverydayEnglishData {
    String content;
    String note;
};

struct FortuneData {
    String sign;
    String description;
    String luckyColor;
    int luckyNumber;
    String message; // For error/info messages like "今天已经抽过签了"
    bool success; // To check if data was successfully retrieved
};

struct ShiciData {
    String content;
    String author;
    String dynasty;
};

struct DuilianData {
    String content;
};

struct FxRateData {
    String money;
};

struct RandomEnWordData {
    String headWord;
    String tranCn;
    String phrases_en; // Concatenated phrases
    String phrases_cn; // Concatenated phrases
};

// Global data storage for Internet menu
extern HealthTipData g_health_tip_data;
extern SayLoveData g_say_love_data;
extern EverydayEnglishData g_everyday_english_data;
extern FortuneData g_fortune_data;
extern ShiciData g_shici_data;
extern DuilianData g_duilian_data;
extern FxRateData g_fx_rate_data;
extern RandomEnWordData g_random_en_word_data;

// Function declarations
void internet_menu_init(); // No longer takes TFT_eSPI& as argument, will use extern tft
void internet_menu_loop();
void internet_menu_draw();
void internet_menu_next_page();
void internet_menu_prev_page();
bool internet_menu_back_press(); // For long press to exit
void InternetMenuScreen(); // New function to encapsulate init and loop

// Individual API fetch functions
void fetchHealthTip();
void fetchSayLove();
void fetchEverydayEnglish();
void fetchFortune();
void fetchShici();
void fetchDuilian();
void fetchFxRate();
void fetchRandomEnWord();

#endif // INTERNET_H
