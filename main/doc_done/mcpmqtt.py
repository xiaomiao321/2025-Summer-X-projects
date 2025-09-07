# 导入必要的模块
import sys
import logging
import paho.mqtt.client as mqtt
import threading
from mcp.server.fastmcp import FastMCP

# --- Configuration ---
# Make sure this matches your ESP32's configuration
MQTT_BROKER = "bemfa.com"  # Using the same broker as the device
MQTT_PORT = 9501           # Using the same port as the device
MQTT_TOPIC = "Light00"     # The topic your device subscribes to
MQTT_CLIENT_ID = "python_mcp_controller" # Use a unique client ID

# --- End Configuration ---

# 创建日志记录器
logger = logging.getLogger('DeviceController')
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# 如果是Windows平台，修复控制台的UTF-8编码问题
if sys.platform == 'win32':
    sys.stderr.reconfigure(encoding='utf-8')
    sys.stdout.reconfigure(encoding='utf-8')

class DeviceController:
    def __init__(self):
        """初始化设备控制器"""
        self.mqtt_client = None
        self.connected = False
        self.lock = threading.Lock()
        self._init_mqtt_client()
        self.mqtt_thread = threading.Thread(target=self._run_mqtt_client, daemon=True)
        self.mqtt_thread.start()

    def _init_mqtt_client(self):
        """初始化MQTT客户端"""
        self.mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
        self.mqtt_client.on_connect = self._on_mqtt_connect
        self.mqtt_client.on_disconnect = self._on_mqtt_disconnect

    def _run_mqtt_client(self):
        """在独立线程中运行MQTT客户端"""
        try:
            self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.mqtt_client.loop_forever(retry_first_connection=True)
        except Exception as e:
            logger.error(f"MQTT客户端线程异常: {str(e)}")
            self.connected = False

    def _on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT连接回调"""
        if rc == 0:
            logger.info("成功连接到MQTT代理")
            self.connected = True
        else:
            logger.error(f"MQTT连接失败，错误代码: {rc}")
            self.connected = False

    def _on_mqtt_disconnect(self, client, userdata, rc):
        """MQTT断开连接回调"""
        logger.warning(f"MQTT连接断开，错误代码: {rc}")
        self.connected = False

    def send_raw_command(self, command: str):
        """发送原始字符串命令到MQTT主题"""
        with self.lock:
            if not self.connected:
                logger.error("无法发送命令: MQTT未连接")
                return False
            try:
                self.mqtt_client.publish(
                    topic=MQTT_TOPIC,
                    payload=command.encode('utf-8'), # Send as raw string
                    qos=1,
                    retain=False
                )
                logger.info(f"已发送命令: '{command}' 到主题 '{MQTT_TOPIC}'")
                return True
            except Exception as e:
                logger.error(f"发送MQTT命令失败: {str(e)}")
                return False

# --- FastMCP Tool Integration ---

# 创建全局控制器实例
device_controller = DeviceController()

# 创建FastMCP服务器实例
mcp = FastMCP("DeviceController")

@mcp.tool()
def send_command(command: str) -> dict:
    """
    发送一个命令到ESP32设备.

    Args:
        command: The command to send.

    Available commands:
    - **exit**: Exits the current sub-menu on the device.

    - **Menu Commands**:
        - "Clock"
        - "Countdown"
        - "Stopwatch"
        - "Music"
        - "Performance"
        - "Temperature"
        - "Animation"
        - "Games"
        - "LED"
        - "ADC"

    - **Song Commands**:
        - "Cai Bu Tou"
        - "Chun Jiao Yu Zhi Ming"
        - "Cheng Du"
        - "Hai Kuo Tian Kong"
        - "Hong Dou"
        - "Hou Lai"
        - "Kai Shi Dong Le"
        - "Lv Se"
        - "Mi Ren De Wei Xian"
        - "Qing Hua Ci"
        - "Xin Qiang"
        - "You Dian Tian"
        - "Windows XP"
    """
    try:
        success = device_controller.send_raw_command(command)
        if success:
            return {"status": "success", "message": f"Command '{command}' sent."}
        else:
            return {"status": "failure", "message": "Failed to send command. MQTT not connected."}
    except Exception as e:
        logger.error(f"Error sending command: {str(e)}")
        return {"status": "error", "message": str(e)}

if __name__ == "__main__":
    # This allows running the script directly to test sending a command
    if len(sys.argv) > 1:
        cmd_to_send = " ".join(sys.argv[1:])
        print(f"Attempting to send command: '{cmd_to_send}'")
        # We need to wait a bit for the MQTT client to connect
        import time
        time.sleep(2) 
        result = send_command(cmd_to_send)
        print(f"Result: {result}")
        # Exit after sending command
        sys.exit(0)

    # If no command-line argument, run the FastMCP server
    mcp.run(transport="stdio")
