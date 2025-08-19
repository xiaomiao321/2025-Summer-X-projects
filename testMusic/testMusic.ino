#define BUZZER_PIN 5  // 无源蜂鸣器接GPIO5

// 音阶频率（Hz），基于12平均律
#define NOTE_C4  262  // Do
#define NOTE_D4  294  // Re
#define NOTE_E4  330  // Mi
#define NOTE_F4  349  // Fa
#define NOTE_G4  392  // So
#define NOTE_A4  440  // La
#define NOTE_B4  494  // Si
#define NOTE_C5  523  // 高音Do

// 小星星音符序列
int melody[] = {
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,  // C C G G A A G
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4,  // F F E E D D C
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4,  // G G F F E E D
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4,  // G G F F E E D
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,  // C C G G A A G
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4   // F F E E D D C
};

// 音符节拍（毫秒），全音符=1000ms，半音符=500ms
int noteDurations[] = {
  500, 500, 500, 500, 500, 500, 1000,  // 第一行
  500, 500, 500, 500, 500, 500, 1000,  // 第二行
  500, 500, 500, 500, 500, 500, 1000,  // 第三行
  500, 500, 500, 500, 500, 500, 1000,  // 第四行
  500, 500, 500, 500, 500, 500, 1000,  // 第五行
  500, 500, 500, 500, 500, 500, 1000   // 第六行
};

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);  // 设置GPIO5为输出
  Serial.begin(115200);         // 串口初始化，用于调试
  Serial.println("无源蜂鸣器播放《小星星》测试程序启动");
}

void loop() {
  // 遍历音符数组，播放整首曲子
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    tone(BUZZER_PIN, melody[i], noteDurations[i]);  // 播放当前音符
    delay(noteDurations[i] * 1.3);  // 音符间稍加间隔（1.3倍持续时间，模拟自然停顿）
    noTone(BUZZER_PIN);  // 停止声音
  }
  
  Serial.println("《小星星》一轮播放完成");
  delay(2000);  // 每轮播放后暂停2秒
}