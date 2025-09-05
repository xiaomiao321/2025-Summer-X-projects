#ifndef BUZZER_H
#define BUZZER_H

#define BUZZER_PIN 5

#include <Arduino.h>

// 播放模式枚举
enum PlayMode {
  SINGLE_LOOP,   // 单曲循环
  LIST_LOOP,     // 列表播放
  RANDOM_PLAY    // 随机播放
};
#include <TFT_eSPI.h>
#include "Music_processed/cai_bu_tou.h"
#include "Music_processed/cheng_du.h"
#include "Music_processed/hai_kuo_tian_kong.h"
#include "Music_processed/hong_dou.h"
#include "Music_processed/hou_lai.h"
#include "Music_processed/kai_shi_dong_le.h"
#include "Music_processed/lv_se.h"
#include "Music_processed/qing_hua_ci.h"
#include "Music_processed/xin_qiang.h"
#include "Music_processed/you_dian_tian.h"
#include "Music_processed/chun_jiao_yu_zhi_ming.h"
#include "Music_processed/Windows.h"
#include "Music_processed/mi_ren_de_wei_xian.h"

// 音阶频率（Hz）
#define NOTE_REST 0
#define NOTE_G3 196
#define NOTE_A3 220
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
// 音阶频率（Hz）
#define P0 	0	// 休止符频率

#define L1 262  // 低音频率
#define L2 294
#define L3 330
#define L4 349
#define L5 392
#define L6 440
#define L7 494

#define M1 523  // 中音频率
#define M2 587
#define M3 659
#define M4 698
#define M5 784
#define M6 880
#define M7 988

#define H1 1047 // 高音频率
#define H2 1175
#define H3 1319
#define H4 1397
#define H5 1568
#define H6 1760
#define H7 1976


#define DAHAI_TIME_OF_BEAT 714 // 大海节拍时间（ms）
// Song structure from Buzzer.cpp
typedef struct {
  const char* name;
  const int* melody;
  const int* durations;
  int length;
  int colorSchemeIndex;
} Song;

// Exposed for watchface hourly chime
extern const Song songs[] PROGMEM;
extern const int numSongs;
extern volatile bool stopBuzzerTask;
void Buzzer_Task(void *pvParameters);
void Buzzer_PlayMusic_Task(void *pvParameters);
void Buzzer_PlayMusic_Task(void *pvParameters);

// Main menu function
void Buzzer_Init();
void BuzzerMenu();

// New functions for direct playback by name
int findSongIndexByName(const String& name);
void setSongToPlay(int index);
void playSpecificSong();



const int melody_da_hai[]  = {
  L5,L6,M1,M1,M1,M1,L6,L5,M1,M1,M2,M1,L6,M1,M2,M2,M2,M2,M1,L6,M2,M2,M3,M2,M3,M5,M6,M6,M5,M6,M5,M3,M2,M2,M3,M2,M1,L6,L5,L6,M1,M1,M1,M1,L6,M1,L5,L6,M1,M1,M1,M1,L6,L5,M1,M1,M2,M1,L6,M1,M2,M2,M2,M2,M1,L6,M2,M2,M3,M2,M3,M5,M6,M6,M5,M6,H1,M6,M5,M3,M2,M1,L6,L5,L6,M1,M1,M1,M1,M2,M1,M1,M3,M5
};
const int durations_da_hai[] = {
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/4,DAHAI_TIME_OF_BEAT/4,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT*3,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,
  
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT*3,DAHAI_TIME_OF_BEAT/2,
};

// 生日快乐
const int melody_happy_birthday[] = {
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_F4, NOTE_E4,
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_G4, NOTE_F4,
  NOTE_C4, NOTE_C4, NOTE_C5, NOTE_A4, NOTE_F4, NOTE_E4, NOTE_D4,
  NOTE_A4, NOTE_A4, NOTE_A4, NOTE_F4, NOTE_G4, NOTE_F4
};
const int durations_happy_birthday[] = {
  250, 250, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 1000
};

#endif
