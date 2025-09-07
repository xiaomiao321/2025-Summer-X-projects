#include <Arduino.h>
#include "ADC.h"
#include "Alarm.h"
#include "esp_adc_cal.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include "Menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include <math.h>
#include "driver/adc.h"
MeterWidget volts = MeterWidget(&tft);

#define BAT_PIN         2
#define ADC_CHANNEL     ADC1_CHANNEL_2
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define ADC_WIDTH       ADC_WIDTH_BIT_12

static esp_adc_cal_characteristics_t adc1_chars;
bool cali_enable = false;
bool stopADCTask = false;

static bool adc_calibration_init() {
    esp_err_t ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) return false;
    if (ret == ESP_ERR_INVALID_VERSION) return false;
    if (ret == ESP_OK) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc1_chars);
        return true;
    }
    return false;
}

void setupADC() {
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    cali_enable = adc_calibration_init();
}

void ADC_Task(void *pvParameters) {
    volts.analogMeter(0, 0, 3.3f, "V", "0", "0.8", "1.6", "2.4", "3.3");

    const float R_FIXED = 20000.0f;
    const float R10 = 8000.0f;
    const float GAMMA = 0.6f;

    while (!stopADCTask) {
        uint32_t sum = 0;
        const int samples = 50;
        for (int i = 0; i < samples; i++) {
            sum += adc1_get_raw(ADC_CHANNEL);
            delay(1);
        }
        sum /= samples;

        float voltage_v = 0;
        if (cali_enable) {
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
            voltage_v = voltage_mv / 1000.0f;
        } else {
            voltage_v = (sum * 3.3) / 4095.0;
        }
        volts.updateNeedle(voltage_v, 0);

        // --- Flicker-free display updates using sprite ---
        menuSprite.fillSprite(TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

        // Calculate and display Lux
        float r_photo = (voltage_v * R_FIXED) / (3.3f - voltage_v);
        float lux = pow((r_photo / R10), (1.0f / -GAMMA)) * 10.0f;

        char luxStr[10];
        dtostrf(lux, 4, 1, luxStr);
        menuSprite.setCursor(20, 10);
        menuSprite.print("LUX: "); menuSprite.print(luxStr);

        // Display Voltage and ADC Raw Value
        char voltStr[10];
        dtostrf(voltage_v, 4, 2, voltStr);
        menuSprite.setCursor(20, 35);
        menuSprite.print("VOL: "); menuSprite.print(voltStr);

        menuSprite.setCursor(20, 60);
        menuSprite.print("ADC: "); menuSprite.print(sum);

        // Draw Progress Bar
        float constrainedLux = constrain(lux, 0.0f, 1000.0f); //
        int barWidth = map((long)constrainedLux, 0L, 100L, 0L, 200L); 
        menuSprite.drawRect(20, 85, 202, 22, TFT_WHITE);
        menuSprite.fillRect(21, 86, 200, 20, TFT_BLACK);
        menuSprite.fillRect(21, 86, barWidth, 20, TFT_GREEN);

        menuSprite.pushSprite(0, 130);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

void ADCMenu() {
    stopADCTask = false;
    tft.fillScreen(TFT_BLACK);
    
    xTaskCreatePinnedToCore(ADC_Task, "ADC_Task", 4096, NULL, 1, NULL, 0);

    while (1) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
            break;
        }
        if (g_alarm_is_ringing) { // ADDED LINE
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
            break; // Exit loop
        }
        if (readButton()) {
            stopADCTask = true;
            vTaskDelay(pdMS_TO_TICKS(200)); 
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}

    