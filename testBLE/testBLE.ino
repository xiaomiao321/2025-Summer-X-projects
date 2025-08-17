#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// BLE 服务和特征 UUID
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea258faaab2f"

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCharacteristic = nullptr;

bool deviceConnected = false;
uint32_t value = 0;

// 回调类：处理连接/断开
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = true;
    Serial.print("Client connected - Handle: ");
    Serial.println(desc->conn_handle);

    // ✅ 正确方式：使用 pServer->updateConnParams(...)（注意：是成员函数）
    pServer->updateConnParams(
        desc->conn_handle,   // 连接句柄
        12,                  // min interval (15ms)
        12,                  // max interval (15ms)
        0,                   // latency
        600                  // timeout (6 seconds)
    );
  }

  void onDisconnect(NimBLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected, restarting advertising");
    pServer->startAdvertising();
  }
};

// 写入回调
class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) {
    std::string rxValue = pChar->getValue();
    if (rxValue.length() > 0) {
      Serial.print("Received from phone: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.printf("%c", rxValue[i]);
      }
      Serial.println();
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting BLE work!");

  // 初始化 BLE 设备
  NimBLEDevice::init("ESP32_BLE_Test");

  // ✅ 设置发射功率：9 = -12dBm（更低功耗），范围 0~9（9 最低）
  NimBLEDevice::setPower(9); // 或使用 NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEDevice::setSecurityAuth(false, false, false); // 可选：关闭认证

  // 创建服务器
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 创建服务
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // 创建特征
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      NIMBLE_PROPERTY::READ   |
                      NIMBLE_PROPERTY::WRITE  |
                      NIMBLE_PROPERTY::NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("Hello from ESP32!");

  // 启动服务
  pService->start();

  // 开始广播
  NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);

  // ✅ 设置广播间隔（单位：0.625ms）—— 增大间隔更省电
  pAdvertising->setMinInterval(1280); // 1.28秒
  pAdvertising->setMaxInterval(1280);

  // ✅ 如果你想设置扫描响应数据（可选）
  // NimBLEAdvertisementData scanResponse;
  // scanResponse.setName("ESP32_BLE_Test");
  // pAdvertising->setScanResponseData(scanResponse);

  pAdvertising->start();

  Serial.println("BLE advertising started! Connect via phone now.");
}

void loop() {
  // 模拟数据发送
  if (deviceConnected) {
    value++;
    std::string newValue = "Counter: " + std::to_string(value);
    pCharacteristic->setValue(newValue);
    pCharacteristic->notify(); // 发送通知
    Serial.printf("Notified: %s\n", newValue.c_str());
  }

  delay(1000); // 可改为低功耗延时或深度睡眠
}