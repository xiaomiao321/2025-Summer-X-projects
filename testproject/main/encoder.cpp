#include "encoder.h"

volatile int encoderPos = 0;
int activeTab = 0;
int buttonPressCount = 0;
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 100;
int lastButtonState = HIGH;
uint8_t lastEncoded = 0;
int deltaSum = 0;

void initEncoder() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastEncoded = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);
  Serial.println("Rotary encoder initialized");
}

void Encoder_Task(void *pvParameters) {
  for (;;) {
    int clk = digitalRead(ENCODER_CLK);
    int dt = digitalRead(ENCODER_DT);
#if SWAP_CLK_DT
    int temp = clk;
    clk = dt;
    dt = temp;
#endif

    uint8_t currentEncoded = (clk << 1) | dt;
    if (currentEncoded != lastEncoded) {
      static const int8_t transitionTable[] = {0, 1, -1, 0,  -1, 0,  0, 1,
                                               1, 0, 0,  -1, 0,  -1, 1, 0};
      int8_t delta = transitionTable[(lastEncoded << 2) | currentEncoded];
      deltaSum += delta;

      if (deltaSum >= 4) {
        encoderPos++;
        activeTab =
            (activeTab + 1) % 2; // Switch between 0 (Time) and 1 (Performance)
        Serial.printf("Clockwise rotation, activeTab: %d\n", activeTab);
        deltaSum = 0;
      } else if (deltaSum <= -4) {
        encoderPos--;
        activeTab = (activeTab - 1 + 2) % 2;
        Serial.printf("Counterclockwise rotation, activeTab: %d\n", activeTab);
        deltaSum = 0;
      }

      lastEncoded = currentEncoded;
    }

    int buttonState = digitalRead(ENCODER_SW);
    if (buttonState != lastButtonState) {
      if (buttonState == LOW && millis() - lastButtonTime > debounceDelay) {
        buttonPressCount++;
        activeTab = (activeTab + 1) % 2; // Toggle tab on button press
        Serial.printf("Button pressed, activeTab: %d\n", activeTab);
        lastButtonTime = millis();
      }
      lastButtonState = buttonState;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}