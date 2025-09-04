#include <NimBLEDevice.h>

// 创建 BLE UART 实例
BLEUart bleUart;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE UART Server...");
  
  // 初始化 BLE
  NimBLEDevice::init("ESP32C3-UART");
  
  // 创建 BLE 服务器并设置 UART 服务
  NimBLEServer* pServer = NimBLEDevice::createServer();
  bleUart.begin(pServer);
  
  // 开始广播
  pServer->getAdvertising()->start();
  
  Serial.println("BLE UART Server started! Waiting for connections...");
  Serial.println("Connect with nRF Connect or similar BLE terminal app");
  Serial.println("Look for service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
}

void loop() {
  // 检查是否有数据从手机通过 BLE 发送过来
  if (bleUart.available()) {
    String receivedData = bleUart.readString();
    Serial.print("Received from BLE: ");
    Serial.println(receivedData);
  }

  
  delay(10);
}