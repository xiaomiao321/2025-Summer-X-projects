#include "animation.h"
#include "Alarm.h"
#include "Menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "Buzzer.h" // For BUZZER_PIN
#include "LED.h" // Include LED.h to use the global strip object

// Global flag to signal task to stop
volatile bool stopAnimationTask = false;
TaskHandle_t animationTaskHandle = NULL;

void Animation_task(void *pvParameters)
{
  int delay_ms = 500;

  while(!stopAnimationTask) // Check the flag
  {
    // 1. Draw the animation frame
    uint16_t fg_color = random(0x10000);
    uint16_t bg_color = TFT_BLACK;
    uint16_t x = random(tft.width());
    uint16_t y = random(tft.height());
    uint8_t radius = random(20, tft.width()/4);
    uint8_t thickness = random(1, radius / 4);
    uint8_t inner_radius = radius - thickness;
    uint16_t start_angle = random(361);
    uint16_t end_angle = random(361);
    bool arc_end = random(2);
    tft.drawSmoothArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, arc_end);

    // 2. Update NeoPixels
    // Convert the 16-bit TFT color (5-6-5) to a 24-bit RGB color for the NeoPixels
    uint8_t r = (fg_color & 0xF800) >> 8;
    uint8_t g = (fg_color & 0x07E0) >> 3;
    uint8_t b = (fg_color & 0x001F) << 3;
    strip.fill(strip.Color(r, g, b)); // Use the global strip object
    strip.show();

    // 3. Play a sound effect using tone() with a duration
    tone(BUZZER_PIN, random(800, 1500), delay_ms);

    // 4. Control animation speed
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    if (delay_ms > 50) {
      delay_ms -= 10;
    }
  }

  // Cleanup before exiting
  noTone(BUZZER_PIN);
  strip.clear();
  strip.show();
  animationTaskHandle = NULL; // Clear the handle
  vTaskDelete(NULL); // Delete self
}

void AnimationMenu()
{
  tft.fillScreen(TFT_BLACK);
  
  // Initialize NeoPixel strip
  strip.show(); // Initialize all pixels to 'off'

  stopAnimationTask = false; // Reset the flag
  
  // Make sure no old task is running
  if(animationTaskHandle != NULL) {
    vTaskDelete(animationTaskHandle);
    animationTaskHandle = NULL;
  }

  xTaskCreatePinnedToCore(Animation_task, "Animation_task", 2048, NULL, 1, &animationTaskHandle, 0);

  initRotaryEncoder();

  while(true)
  {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break; // Exit loop
    }
    if(readButton())
    {
      // Signal the task to stop
      if (animationTaskHandle != NULL) {
        stopAnimationTask = true;
      }

      // Wait for the task to delete itself
      vTaskDelay(pdMS_TO_TICKS(200)); 

      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
    // display = 48;
    // picture_flag = 0;
    // showMenuConfig();
}