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
// 定义蜂鸣器连接的引脚
#define BUZZER_PIN 5 // 请根据您的硬件连接修改此引脚号

// 定义 Song 结构体
typedef struct {
  const char* name;
  const int* melody;
  const int* durations;
  int length;
} Song;


const Song songs[] PROGMEM= {
  { "Cai Bu Tou", melody_cai_bu_tou, durations_cai_bu_tou, sizeof(melody_cai_bu_tou) / sizeof(melody_cai_bu_tou[0]) },
  { "Chun Jiao Yu Zhi Ming",melody_chun_jiao_yu_zhi_ming,durations_chun_jiao_yu_zhi_ming,sizeof(melody_chun_jiao_yu_zhi_ming)/sizeof(melody_chun_jiao_yu_zhi_ming[0])},
  { "Cheng Du", melody_cheng_du, durations_cheng_du, sizeof(melody_cheng_du)/sizeof(melody_cheng_du[0]) },
  { "Hai Kuo Tian Kong",melody_hai_kuo_tian_kong,durations_hai_kuo_tian_kong, sizeof(melody_hai_kuo_tian_kong)/sizeof(melody_hai_kuo_tian_kong[0]) },
  { "Hong Dou", melody_hong_dou, durations_hong_dou, sizeof(melody_hong_dou)/sizeof(melody_hong_dou[0])},
  { "Hou Lai", melody_hou_lai, durations_hou_lai, sizeof(melody_hou_lai)/sizeof(melody_hou_lai[0])},
  { "Kai Shi Dong Le", melody_kai_shi_dong_le, durations_kai_shi_dong_le, sizeof(melody_kai_shi_dong_le)/sizeof(melody_kai_shi_dong_le[0])},
  { "Lv Se", melody_lv_se, durations_lv_se, sizeof(melody_lv_se)/sizeof(melody_lv_se[0])},
  { "Mi Ren De Wei Xian",melody_mi_ren_de_wei_xian,durations_mi_ren_de_wei_xian,sizeof(melody_mi_ren_de_wei_xian)/sizeof(melody_mi_ren_de_wei_xian[0])},
  { "Qing Hua Ci", melody_qing_hua_ci, durations_qing_hua_ci, sizeof(melody_qing_hua_ci)/sizeof(melody_qing_hua_ci[0])},
  { "Xin Qiang", melody_xin_qiang, durations_xin_qiang,sizeof(melody_xin_qiang)/sizeof(melody_xin_qiang[0])},
  { "You Dian Tian", melody_you_dian_tian, durations_you_dian_tian, sizeof(melody_you_dian_tian)/sizeof(melody_you_dian_tian[0])},
  // {"Da Hai", melody_da_hai, durations_da_hai, sizeof(melody_da_hai) / sizeof(melody_da_hai[0])}, 
  // {"Happy Birthday", melody_happy_birthday, durations_happy_birthday, sizeof(melody_happy_birthday) / sizeof(melody_happy_birthday[0])}, 
  { "Windows XP",melody_windows,durations_windows,sizeof(melody_windows) / sizeof(melody_windows[0])},
};
const int numSongs = sizeof(songs) / sizeof(songs[0]);

// 播放音乐函数
void playSong(int songIndex) {
  // 检查索引是否有效
  if (songIndex < 0 || songIndex >= numSongs) {
    Serial.print("Invalid song index: ");
    Serial.println(songIndex);
    return;
  }

  Song song;
  memcpy_P(&song, &songs[songIndex], sizeof(Song));

  Serial.print("Playing song: ");
  Serial.println(song.name);

  // 播放旋律
  for (int i = 0; i < song.length; i++) {
    // 从 PROGMEM 读取音符和持续时间
    int note = pgm_read_word(&(song.melody[i]));
    int duration = pgm_read_word(&(song.durations[i]));

    Serial.print("Note: ");
    Serial.print(note);
    Serial.print(", Duration: ");
    Serial.println(duration);

    // 播放音符
    tone(BUZZER_PIN, note, duration);

    // 等待音符播放完成，并留一点间隔
    delay(duration * 1.1); // 增加10%间隔时间

    // 确保蜂鸣器在下一个音符前关闭
    noTone(BUZZER_PIN);
  }

  Serial.print("Finished playing song: ");
  Serial.println(song.name);
}

void setup() {
  // 初始化串口监视器
  Serial.begin(115200);
  Serial.println("Buzzer Music Player Started");

  // 设置蜂鸣器引脚为输出
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  // 循环播放所有歌曲
  for (int i = 0; i < numSongs; i++) {
    playSong(i);
    delay(1000); // 每首歌之间暂停1秒
  }
  Serial.println("All songs played once. Restarting...");
  delay(2000); // 全部播放完后暂停2秒再重新开始
}