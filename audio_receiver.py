#!/usr/bin/env python3
"""
ESP32 麥克風串口音頻接收器
接收從 ESP32 傳來的音頻數據並播放到喇叭
"""

import serial
import pyaudio
import numpy as np
import threading
import time

# 音頻參數（必須與 ESP32 端一致）
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2  # 16 bits = 2 bytes
BUFFER_SIZE = 128 * 2  # 128 samples * 2 bytes

# 串口參數
SERIAL_PORT = 'COM3'  # 根據您的系統調整
BAUD_RATE = 115200

class AudioStreamer:
    def __init__(self):
        self.serial_conn = None
        self.audio = None
        self.stream = None
        self.running = False
        
    def setup_serial(self):
        """設定串口連接"""
        try:
            self.serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            print(f"✓ 串口連接成功: {SERIAL_PORT}")
            return True
        except Exception as e:
            print(f"✗ 串口連接失敗: {e}")
            return False
    
    def setup_audio(self):
        """設定音頻輸出"""
        try:
            self.audio = pyaudio.PyAudio()
            self.stream = self.audio.open(
                format=pyaudio.paInt16,
                channels=CHANNELS,
                rate=SAMPLE_RATE,
                output=True,
                frames_per_buffer=BUFFER_SIZE // 2
            )
            print(f"✓ 音頻系統初始化成功")
            return True
        except Exception as e:
            print(f"✗ 音頻系統初始化失敗: {e}")
            return False
    
    def audio_thread(self):
        """音頻接收和播放線程"""
        print("開始音頻串流...")
        buffer = b''
        
        while self.running:
            try:
                # 從串口讀取數據
                data = self.serial_conn.read(BUFFER_SIZE)
                if len(data) > 0:
                    buffer += data
                    
                    # 當緩衝區有足夠數據時播放
                    while len(buffer) >= BUFFER_SIZE:
                        audio_chunk = buffer[:BUFFER_SIZE]
                        buffer = buffer[BUFFER_SIZE:]
                        
                        # 播放音頻
                        self.stream.write(audio_chunk)
                        
            except Exception as e:
                print(f"音頻處理錯誤: {e}")
                break
    
    def start(self):
        """開始音頻串流"""
        if not self.setup_serial():
            return False
            
        if not self.setup_audio():
            return False
        
        self.running = True
        
        # 啟動音頻處理線程
        audio_thread = threading.Thread(target=self.audio_thread)
        audio_thread.daemon = True
        audio_thread.start()
        
        print("音頻串流已啟動，按 Ctrl+C 停止...")
        return True
    
    def stop(self):
        """停止音頻串流"""
        self.running = False
        
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
        
        if self.audio:
            self.audio.terminate()
        
        if self.serial_conn:
            self.serial_conn.close()
        
        print("音頻串流已停止")

def main():
    streamer = AudioStreamer()
    
    try:
        if streamer.start():
            while True:
                time.sleep(1)
    except KeyboardInterrupt:
        print("\n收到停止信號...")
    finally:
        streamer.stop()

if __name__ == "__main__":
    print("=== ESP32 麥克風音頻接收器 ===")
    print("請確保已安裝以下 Python 套件:")
    print("pip install pyaudio pyserial numpy")
    print("===============================\n")
    
    main()
