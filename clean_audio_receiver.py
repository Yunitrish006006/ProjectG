#!/usr/bin/env python3
"""
ESP32 MQTT 音訊接收器 - 無警告版本
抑制所有非關鍵警告，提供更乾淨的輸出體驗
"""

import warnings
import os

# 抑制所有警告
warnings.filterwarnings('ignore')

# 抑制 matplotlib 字體警告
os.environ['PYTHONWARNINGS'] = 'ignore'

# 現在導入主程序
try:
    from realtime_audio_receiver import ESP32AudioReceiver
    
    def main():
        print("🎤 ESP32 MQTT 即時音訊接收器 (無警告版本)")
        print("=" * 50)
        
        # 選擇 MQTT 服務器
        servers = [
            "broker.emqx.io",
            "test.mosquitto.org", 
            "broker.hivemq.com"
        ]
        
        print("可用的 MQTT 服務器:")
        for i, server in enumerate(servers, 1):
            print(f"{i}. {server}")
        
        # 使用默認服務器
        server = servers[0]
        print(f"使用服務器: {server}")
        
        # 創建並啟動音訊接收器
        receiver = ESP32AudioReceiver(mqtt_host=server)
        receiver.run()
    
    if __name__ == "__main__":
        main()
        
except ImportError as e:
    print(f"❌ 導入錯誤: {e}")
    print("請確保 realtime_audio_receiver.py 文件存在且可訪問")
except Exception as e:
    print(f"❌ 運行錯誤: {e}")
