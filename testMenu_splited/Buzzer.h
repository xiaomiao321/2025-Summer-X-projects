#ifndef BUZZER_H
#define BUZZER_H
#define BUZZER_PIN 5
#include "Menu.h"
#include <Arduino.h> // For int, bool, etc.
#include <stdint.h> // For uint16_t

// // 音阶频率（Hz）
// #define NOTE_REST 0
// #define NOTE_G3 196
// #define NOTE_A3 220
// #define NOTE_B3 247
// #define NOTE_C4 262
// #define NOTE_CS4 277
// #define NOTE_D4 294
// #define NOTE_DS4 311
// #define NOTE_E4 330
// #define NOTE_F4 349
// #define NOTE_FS4 370
// #define NOTE_G4 392
// #define NOTE_GS4 415
// #define NOTE_A4 440
// #define NOTE_AS4 466
// #define NOTE_B4 494
// #define NOTE_C5 523
// #define NOTE_CS5 554
// #define NOTE_D5 587
// #define NOTE_DS5 622
// #define NOTE_E5 659
// #define NOTE_F5 698
// #define NOTE_FS5 740
// #define NOTE_G5 784
// #define NOTE_GS5 831
// #define NOTE_A5 880
// #define NOTE_AS5 932
// #define NOTE_B5 988
// #define P0 	0	// 休止符频率

// #define L1 262  // 低音频率
// #define L2 294
// #define L3 330
// #define L4 349
// #define L5 392
// #define L6 440
// #define L7 494

// #define M1 523  // 中音频率
// #define M2 587
// #define M3 659
// #define M4 698
// #define M5 784
// #define M6 880
// #define M7 988

// #define H1 1047 // 高音频率
// #define H2 1175
// #define H3 1319
// #define H4 1397
// #define H5 1568
// #define H6 1760
// #define H7 1976


// #define DAHAI_TIME_OF_BEAT 714 // 大海节拍时间（ms）

// #define YUJIAN_TIME_OF_BEAT 652 // 大海节拍时间（ms）


// #define DOUBLE_CLICK_TIME 500 // 双击时间（ms）

// #define NUM_BANDS 16

// extern double spectrum[NUM_BANDS];

// // Song data structure
// typedef struct {
//   const char* name; // Song name
//   int* melody;      // Note sequence
//   int* durations;   // Note durations
//   int length;       // Number of notes
//   int colorSchemeIndex; // New: Index to colorSchemes array
// } Song;

// // Color scheme structure
// typedef struct {
//   uint16_t bgColorStart; // Background gradient start color
//   uint16_t bgColorEnd;   // Background gradient end color
//   uint16_t frameColor1;  // Frame color 1
//   uint16_t frameColor2;  // Frame color 2
//   uint16_t frameColor3;  // Frame color 3
//   uint16_t textColor;    // Normal text color
//   uint16_t highlightColor; // Highlighted text color
// } ColorScheme;

// // Extern declarations for melodies and durations
// extern int melody_da_hai[];
// extern int durations_da_hai[];
// extern int melody_happy_birthday[];
// extern int durations_happy_birthday[];
// extern int melody_cai_bu_tou[];
// extern int durations_cai_bu_tou[];
// extern int melody_cheng_du[];
// extern int durations_cheng_du[];
// extern int melody_chun_jiao_yu_zhi_ming[];
// extern int durations_chun_jiao_yu_zhi_ming[];
// extern int melody_dao_xiang[];
// extern int durations_dao_xiang[];
// extern int melody_gao_bai_qi_qiu[];
// extern int durations_gao_bai_qi_qiu[];
// extern int melody_guang_hui_sui_yue[];
// extern int durations_guang_hui_sui_yue[];
// extern int melody_hong_dou[];
// extern int durations_hong_dou[];
// extern int melody_hong_se_gao_gen_xie[];
// extern int durations_hong_se_gao_gen_xie[];
// extern int melody_hou_lai[];
// extern int durations_hou_lai[];
// extern int melody_jiang_nan[];
// extern int durations_jiang_nan[];
// extern int melody_kai_shi_dong_le[];
// extern int durations_kai_shi_dong_le[];
// extern int melody_lv_se[];
// extern int durations_lv_se[];
// extern int melody_mi_ren_de_wei_xian[];
// extern int durations_mi_ren_de_wei_xian[];
// extern int melody_qing_hua_ci[];
// extern int durations_qing_hua_ci[];
// extern int melody_shi_nian[];
// extern int durations_shi_nian[];
// extern int melody_su_yan[];
// extern int durations_su_yan[];
// extern int melody_xin_qiang[];
// extern int durations_xin_qiang[];
// extern int melody_yan_yuan[];
// extern int durations_yan_yuan[];
// extern int melody_you_dian_tian[];
// extern int durations_you_dian_tian[];
// extern int melody_yu_jian[];
// extern int durations_yu_jian[];
// extern int melody_jingle_bells[];
// extern int durations_jingle_bells[];


// // Extern declarations for song list and color schemes
// extern Song songs[];
// extern const int numSongs;
// extern const ColorScheme colorSchemes[];
// extern const int numColorSchemes;

void BuzzerMenu();
bool detectDoubleClick();


#endif