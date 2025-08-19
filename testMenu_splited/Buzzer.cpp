#include "buzzer.h"

bool stopBuzzerTask = false;
// 音阶频率（Hz）
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

// 小星星音符序列
int melody[] = {
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4,
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4,
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4,
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4
};

// 音符节拍（毫秒）
int noteDurations[] = {
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000
};

void Buzzer_Init_Task(void *pvParameters) {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("无源蜂鸣器初始化完成");
  vTaskDelete(NULL);
}

void Buzzer_Task(void *pvParameters) {
  for (;;) {
    // 检查是否需要提前停止
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);  // 确保关闭蜂鸣器
      stopBuzzerTask = false;  // 重置标志
      Serial.println("Buzzer_Task 被外部中断，已停止");
      break;
    }

    for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
      // 再次检查退出标志
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        stopBuzzerTask = false;
        goto exit_loop;
      }

      tone(BUZZER_PIN, melody[i], noteDurations[i]);
      vTaskDelay(pdMS_TO_TICKS(noteDurations[i] * 1.3));
      noTone(BUZZER_PIN);  
    }
    Serial.println("《小星星》一轮播放完成");
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

exit_loop:
  vTaskDelete(NULL);
}
void BuzzerMenu()
{
  stopBuzzerTask = false;  // 确保标志干净
  animateMenuTransition("BUZZER", true);
  xTaskCreatePinnedToCore(Buzzer_Init_Task, "Buzzer_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 8192, NULL, 1, NULL, 0);

  while (1) {
    if (readButton()) {
      Serial.println("检测到按钮，准备停止蜂鸣器...");

      // 设置停止标志
      stopBuzzerTask = true;

      // 等待任务结束
      TaskHandle_t taskHandle = xTaskGetHandle("Buzzer_Task");
      if (taskHandle != NULL) {
        // 等待任务删除自己
        const TickType_t xTimeout = pdMS_TO_TICKS(100); // 最多等100ms
        while (eTaskGetState(taskHandle) != eDeleted && xTimeout > 0) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }

      // 清理 Init 任务（如果还存在）
      TaskHandle_t initHandle = xTaskGetHandle("Buzzer_Init");
      if (initHandle != NULL) {
        vTaskDelete(initHandle);
      }

      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  animateMenuTransition("BUZZER", false);
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}