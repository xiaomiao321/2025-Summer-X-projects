#include "pcdata.h"

struct PCData pcData;
SemaphoreHandle_t xPCDataMutex = NULL;
char inputBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool stringComplete = false;
float esp32c3_temp = 0.0f;

void resetBuffer() {
  bufferIndex = 0;
  inputBuffer[0] = '\0';
}

void parsePCData() {
  PCData newData;

  char *ptr = nullptr;

  // Parse CPU temperature and load
  ptr = strstr(inputBuffer, "C");
  if (ptr) {
    char *degree = strchr(ptr, 'c');
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.cpuTemp = atoi(temp);
      newData.cpuLoad = atoi(load);
    }
  }

  // Parse GPU temperature and load
  ptr = strstr(inputBuffer, "G");
  if (ptr) {
    char *degree = strstr(ptr, "c");
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.gpuTemp = atoi(temp);
      newData.gpuLoad = atoi(load);
    }
  }

  // Parse RAM load
  ptr = strstr(inputBuffer, "RL");
  if (ptr) {
    ptr += 2;
    char *end = strchr(ptr, '|');
    if (end) {
      char load[8] = {0};
      strncpy(load, ptr, end - ptr);
      newData.ramLoad = atof(load);
    }
  }

  // Parse RPM
  ptr = strstr(inputBuffer, "GRPM");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (end && end - ptr < sizeof(newData.rpm) - 1) {
      strncpy(newData.rpm, ptr, end - ptr);
      newData.rpm[end - ptr] = '\0';
    }
  }

  // Parse CPU name
  ptr = strstr(inputBuffer, "CPU:");
  if (ptr) {
    ptr += 4;
    char *end = strstr(ptr, "GPU:");
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.cpuName) - 1) {
      strncpy(newData.cpuName, ptr, end - ptr);
      newData.cpuName[end - ptr] = '\0';
      for (int i = strlen(newData.cpuName) - 1; i >= 0 && newData.cpuName[i] == ' '; i--)
        newData.cpuName[i] = '\0';
    }
  }

  // Parse GPU name
  ptr = strstr(inputBuffer, "GPU:");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.gpuName) - 1) {
      strncpy(newData.gpuName, ptr, end - ptr);
      newData.gpuName[end - ptr] = '\0';
      for (int i = strlen(newData.gpuName) - 1; i >= 0 && newData.gpuName[i] == ' '; i--)
        newData.gpuName[i] = '\0';
    }
  }

  // Mark data validity
  if (newData.cpuTemp > 0 || newData.gpuTemp > 0 || newData.ramLoad > 0) {
    newData.valid = true;
  }

  // Safely update pcData
  if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
    pcData = newData;
    xSemaphoreGive(xPCDataMutex);
  }
}

void SERIAL_Task(void *pvParameters) {
  for (;;) {
    if (Serial.available()) {
      char inChar = (char)Serial.read();

      if (bufferIndex < BUFFER_SIZE - 1) {
        inputBuffer[bufferIndex++] = inChar;
        inputBuffer[bufferIndex] = '\0';
      } else {
        //showError("Buffer full!");
        resetBuffer();
      }

      if (inChar == '\n') {
        stringComplete = true;
      }
    }

    if (stringComplete) {
      parsePCData();
      stringComplete = false;
      resetBuffer();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}