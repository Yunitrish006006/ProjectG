#!/usr/bin/env python3
"""
ESP32 MQTT 即時音訊接收器和播放器
功能：
- 接收 ESP32 透過 MQTT 傳送的音訊數據
- 即時播放音訊到喇叭
- 保存音訊為 WAV 檔案
- 顯示即時音訊波形
- 控制 ESP32 音訊錄製
"""

import paho.mqtt.client as mqtt
import json
import numpy as np
import sounddevice as sd
import wave
import threading
import time
from collections import deque
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib.animation as animation

class ESP32AudioReceiver:
    def __init__(self, mqtt_host="broker.emqx.io", mqtt_port=1883):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        
        # 音訊參數 (與 ESP32 一致)
        self.sample_rate = 16000
        self.channels = 1
        self.sample_width = 2  # 16-bit
        
        # MQTT 客戶端
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
        
        # 音訊緩衝區
        self.audio_buffer = deque()
        self.play_buffer = deque(maxlen=8000)  # 0.5秒緩衝
        self.wave_buffer = deque(maxlen=1600)  # 0.1秒顯示緩衝
        
        # 控制變數
        self.connected = False
        self.recording = False
        self.playing = False
        self.saving = False
        
        # 統計資料
        self.stats = {
            'packets_received': 0,
            'samples_received': 0,
            'start_time': time.time(),
            'last_packet_time': 0
        }
        
        # 音訊保存
        self.wav_file = None
        self.wav_filename = None
        
        # 圖形顯示
        self.plot_enabled = True
        self.fig = None
        self.ax = None
        self.line = None
        
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT 連接回調"""
        if rc == 0:
            self.connected = True
            print(f"✓ 已連接到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}")
            
            # 訂閱音訊主題
            topics = [
                "esp32/audio/raw",
                "esp32/audio/status"
            ]
            
            for topic in topics:
                client.subscribe(topic)
                print(f"✓ 訂閱主題: {topic}")
        else:
            print(f"✗ MQTT 連接失敗，錯誤碼: {rc}")
    
    def on_mqtt_disconnect(self, client, userdata, rc):
        """MQTT 斷開連接回調"""
        self.connected = False
        print(f"MQTT 連接已斷開，錯誤碼: {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        """MQTT 消息處理"""
        try:
            topic = msg.topic
            
            if topic == "esp32/audio/raw":
                data = json.loads(msg.payload.decode('utf-8'))
                self.handle_audio_data(data)
            elif topic == "esp32/audio/status":
                data = json.loads(msg.payload.decode('utf-8'))
                self.handle_status_data(data)
                
        except Exception as e:
            print(f"處理 MQTT 消息時出錯: {e}")
    
    def handle_audio_data(self, data):
        """處理音訊數據"""
        if 'audio' in data and 'timestamp' in data:
            # 將音訊數據轉換為 numpy 陣列
            audio_samples = np.array(data['audio'], dtype=np.int16)
            timestamp = data['timestamp']
            
            # 正規化到 [-1, 1] 範圍供播放使用
            audio_float = audio_samples.astype(np.float32) / 32768.0
            
            # 添加到播放緩衝區
            self.play_buffer.extend(audio_float)
            
            # 添加到顯示緩衝區
            self.wave_buffer.extend(audio_float)
            
            # 如果正在保存，寫入 WAV 檔案
            if self.saving and self.wav_file:
                self.wav_file.writeframes(audio_samples.tobytes())
            
            # 更新統計
            self.stats['packets_received'] += 1
            self.stats['samples_received'] += len(audio_samples)
            self.stats['last_packet_time'] = timestamp
            
            # 每100個包輸出一次統計
            if self.stats['packets_received'] % 100 == 0:
                duration = (time.time() - self.stats['start_time'])
                rate = self.stats['packets_received'] / duration if duration > 0 else 0
                print(f"📊 音訊 - 包: {self.stats['packets_received']}, "
                      f"樣本: {len(audio_samples)}, "
                      f"速率: {rate:.1f} 包/秒, "
                      f"RMS: {np.sqrt(np.mean(audio_float**2)):.3f}")
    
    def handle_status_data(self, data):
        """處理狀態數據"""
        status = data.get('status', 'unknown')
        publishing = data.get('publishing', False)
        feature_extraction = data.get('featureExtraction', False)
        
        print(f"📡 ESP32 狀態: {status}, 發布: {publishing}, 特徵提取: {feature_extraction}")
    
    def audio_callback(self, outdata, frames, time, status):
        """音訊播放回調函數"""
        if status:
            print(f"音訊播放狀態: {status}")
        
        # 從播放緩衝區取得音訊數據
        if len(self.play_buffer) >= frames:
            # 取得足夠的樣本
            samples = []
            for _ in range(frames):
                if self.play_buffer:
                    samples.append(self.play_buffer.popleft())
                else:
                    samples.append(0.0)
            
            outdata[:] = np.array(samples).reshape(-1, 1)
        else:
            # 緩衝區不足，輸出靜音
            outdata.fill(0)
    
    def start_audio_playback(self):
        """開始音訊播放"""
        try:
            self.audio_stream = sd.OutputStream(
                samplerate=self.sample_rate,
                channels=self.channels,
                callback=self.audio_callback,
                blocksize=512
            )
            self.audio_stream.start()
            self.playing = True
            print("🔊 音訊播放已開始")
        except Exception as e:
            print(f"✗ 無法啟動音訊播放: {e}")
    
    def stop_audio_playback(self):
        """停止音訊播放"""
        if hasattr(self, 'audio_stream') and self.audio_stream:
            self.audio_stream.stop()
            self.audio_stream.close()
            self.playing = False
            print("🔇 音訊播放已停止")
    
    def start_recording(self, filename=None):
        """開始保存音訊到檔案"""
        if not filename:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"esp32_audio_{timestamp}.wav"
        
        try:
            self.wav_filename = filename
            self.wav_file = wave.open(filename, 'wb')
            self.wav_file.setnchannels(self.channels)
            self.wav_file.setsampwidth(self.sample_width)
            self.wav_file.setframerate(self.sample_rate)
            self.saving = True
            print(f"💾 開始保存音訊到: {filename}")
        except Exception as e:
            print(f"✗ 無法開始錄音: {e}")
    
    def stop_recording(self):
        """停止保存音訊"""
        if self.saving and self.wav_file:
            self.wav_file.close()
            self.saving = False
            print(f"📁 音訊已保存到: {self.wav_filename}")
            
            # 顯示檔案資訊
            duration = self.stats['samples_received'] / self.sample_rate
            size_mb = self.stats['samples_received'] * self.sample_width / (1024 * 1024)
            print(f"   時長: {duration:.1f}秒, 大小: {size_mb:.1f}MB")
    
    def setup_plot(self):
        """設置即時波形顯示"""
        if not self.plot_enabled:
            return
        
        plt.ion()
        self.fig, self.ax = plt.subplots(figsize=(12, 4))
        self.ax.set_title('ESP32 即時音訊波形')
        self.ax.set_xlabel('樣本')
        self.ax.set_ylabel('振幅')
        self.ax.set_ylim(-1, 1)
        self.ax.grid(True)
        
        # 初始化空的線條
        x = np.arange(len(self.wave_buffer) if self.wave_buffer else 1600)
        y = np.zeros_like(x)
        self.line, = self.ax.plot(x, y)
        
        plt.tight_layout()
        plt.show()
    
    def update_plot(self):
        """更新波形顯示"""
        if not self.plot_enabled or not self.fig:
            return
        
        if len(self.wave_buffer) > 0:
            # 更新波形數據
            y_data = np.array(list(self.wave_buffer))
            x_data = np.arange(len(y_data))
            
            self.line.set_data(x_data, y_data)
            self.ax.set_xlim(0, len(y_data))
            
            # 計算並顯示統計資訊
            if len(y_data) > 0:
                rms = np.sqrt(np.mean(y_data**2))
                peak = np.max(np.abs(y_data))
                self.ax.set_title(f'ESP32 即時音訊波形 - RMS: {rms:.3f}, Peak: {peak:.3f}')
            
            self.fig.canvas.draw()
            self.fig.canvas.flush_events()
    
    def send_control_command(self, command):
        """發送控制命令到 ESP32"""
        if self.connected:
            payload = json.dumps({
                "command": command,
                "timestamp": int(time.time() * 1000)
            })
            self.mqtt_client.publish("esp32/audio/control", payload)
            print(f"📤 發送命令: {command}")
        else:
            print("✗ MQTT 未連接，無法發送命令")
    
    def connect(self):
        """連接到 MQTT 服務器"""
        try:
            print(f"正在連接到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}...")
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            
            # 等待連接
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.1)
                timeout -= 0.1
            
            return self.connected
        except Exception as e:
            print(f"✗ MQTT 連接錯誤: {e}")
            return False
    
    def disconnect(self):
        """斷開 MQTT 連接"""
        self.mqtt_client.loop_stop()
        self.mqtt_client.disconnect()
        self.stop_audio_playback()
        self.stop_recording()
        print("🔌 已斷開連接")
    
    def print_stats(self):
        """輸出統計資訊"""
        duration = time.time() - self.stats['start_time']
        rate = self.stats['packets_received'] / duration if duration > 0 else 0
        
        print("\n" + "="*50)
        print("📊 音訊接收統計")
        print("="*50)
        print(f"連接時間: {duration:.1f}秒")
        print(f"接收包數: {self.stats['packets_received']}")
        print(f"接收樣本: {self.stats['samples_received']}")
        print(f"接收速率: {rate:.1f} 包/秒")
        print(f"播放狀態: {'🔊 播放中' if self.playing else '🔇 停止'}")
        print(f"錄音狀態: {'💾 錄音中' if self.saving else '⏹️ 停止'}")
        print("="*50 + "\n")

def main():
    print("🎤 ESP32 MQTT 即時音訊接收器")
    print("================================")
    print("功能: 接收音訊 | 即時播放 | 保存檔案 | 波形顯示")
    print()
    
    # 可選的 MQTT 服務器
    mqtt_servers = [
        "broker.emqx.io",        # EMQX (預設)
        "test.mosquitto.org",    # Eclipse Mosquitto
        "broker.hivemq.com",     # HiveMQ
    ]
    
    print("可用的 MQTT 服務器:")
    for i, server in enumerate(mqtt_servers, 1):
        print(f"{i}. {server}")
    
    # 使用預設服務器
    selected_server = mqtt_servers[0]
    print(f"\n使用服務器: {selected_server}")
    
    # 建立音訊接收器
    receiver = ESP32AudioReceiver(mqtt_host=selected_server)
    
    try:
        # 連接 MQTT
        if not receiver.connect():
            print("❌ MQTT 連接失敗")
            return
        
        print("⏳ 等待 2 秒讓連接穩定...")
        time.sleep(2)
        
        # 發送控制命令啟動 ESP32 音訊發布
        receiver.send_control_command("startPublishing")
        receiver.send_control_command("enableFeatures")
        time.sleep(1)        
        # 開始音訊播放
        receiver.start_audio_playback()
        
        # 設置即時波形顯示
        receiver.setup_plot()
        
        print("\n" + "="*60)
        print("🎵 音訊接收器已啟動")
        print("="*60)
        print("按鍵指令 (無需按 Enter):")
        print("  r: 開始/停止錄音")
        print("  p: 開始/停止播放") 
        print("  s: 顯示統計資訊")
        print("  q: 退出程式")
        print("  Ctrl+C: 強制退出")
        print("="*60)
          # 主循環
        recording = False
        print("輸入指令 (r/p/s/q):")
        
        while True:
            # 更新波形顯示
            receiver.update_plot()
            
            # Windows 相容的用戶輸入檢查
            import msvcrt
            
            if msvcrt.kbhit():
                command = msvcrt.getch().decode('utf-8').lower()
                print(f"\n收到指令: {command}")
                
                if command == 'q':
                    print("正在退出...")
                    break
                elif command == 'r':
                    if recording:
                        receiver.stop_recording()
                        recording = False
                        print("錄音已停止")
                    else:
                        receiver.start_recording()
                        recording = True
                        print("錄音已開始")
                elif command == 'p':
                    if receiver.playing:
                        receiver.stop_audio_playback()
                        print("播放已停止")
                    else:
                        receiver.start_audio_playback()
                        print("播放已開始")
                elif command == 's':
                    receiver.print_stats()
                
                print("輸入指令 (r/p/s/q):")
            
            time.sleep(0.05)  # 20 FPS 更新率
    
    except KeyboardInterrupt:
        print("\n⏹️ 收到中斷信號，正在停止...")
    
    except Exception as e:
        print(f"❌ 運行錯誤: {e}")
    
    finally:
        # 清理資源
        receiver.send_control_command("stopPublishing")
        time.sleep(0.5)
        receiver.disconnect()
        print("👋 音訊接收器已關閉")

if __name__ == "__main__":
    main()
