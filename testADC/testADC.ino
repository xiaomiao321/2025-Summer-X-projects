#include <Arduino.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

// -------------------------------
// 配置 GPIO2 为 ADC 输入
// -------------------------------
#define BAT_PIN         2
#define ADC_CHANNEL     ADC1_CHANNEL_2  // GPIO2 = ADC1 Channel 2
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define ADC_WIDTH       ADC_WIDTH_BIT_12

static esp_adc_cal_characteristics_t adc1_chars;
bool cali_enable = false;

// ADC 校准初始化
static bool adc_calibration_init() {
    esp_err_t ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        Serial.println("eFuse Two Point: NOT SUPPORTED");
        return false;
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        Serial.println("eFuse Two Point: INVALID VERSION");
        return false;
    } else if (ret == ESP_OK) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc1_chars);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\nADC Test Start - GPIO2");

    // 配置 ADC1 宽度和衰减
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // 初始化校准
    cali_enable = adc_calibration_init();
    if (cali_enable) {
        Serial.println("ADC Calibration enabled");
    } else {
        Serial.println("ADC Calibration failed or not available");
    }
}

void loop() {
    const int samples = 50;
    uint32_t sum = 0;

    // 采集 50 次 ADC 值
    for (int i = 0; i < samples; i++) {
        int raw = adc1_get_raw(ADC_CHANNEL);  // 读取 ADC1 通道值
        if (raw >= 0) {
            sum += raw;
        }
        delay(1);
    }
    sum /= samples;

    // 输出原始值
    Serial.print("ADC Raw: ");
    Serial.println(sum);

    // 如果校准启用，转换为电压（mV）
    if (cali_enable) {
        uint32_t voltage = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
        Serial.print("Voltage: ");
        Serial.print(voltage);
        Serial.println(" mV");
    }

    delay(2000);
}