#!/usr/bin/env python3
"""
ESP32 串行監控器
用於監控 ESP32 的調試輸出
"""

import serial
import serial.tools.list_ports
import time
import sys

def find_esp32_port():
    """自動尋找 ESP32 串行端口"""
    
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'USB' in port.description or 'CP210' in port.description or 'CH340' in port.description:
            return port.device
    
    # 如果找不到，嘗試常見端口
    common_ports = ['COM3', 'COM4', 'COM5', 'COM6', 'COM7', 'COM8', 'COM9', 'COM10']
    for port in common_ports:
        try:
            ser = serial.Serial(port, 115200, timeout=1)
            ser.close()
            return port
        except:
            continue
    
    return None

def monitor_serial(port='COM7', baudrate=115200):
    """監控串行輸出"""
    print(f"🔍 ESP32 串行監控器")
    print(f"📡 端口: {port}")
    print(f"⚡ 波特率: {baudrate}")
    print("=" * 50)
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✅ 成功連接到 {port}")
        print("🎯 監控 ESP32 調試輸出...")
        print("=" * 50)
        
        start_time = time.time()
        line_count = 0
        
        while True:
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        timestamp = time.strftime("%H:%M:%S")
                        print(f"[{timestamp}] {line}")
                        line_count += 1
                        
                        # 高亮重要信息
                        if "MQTT" in line or "音訊" in line or "麥克風" in line:
                            print("  " + "="*60)
                        
                        # 統計信息
                        if line_count % 100 == 0:
                            elapsed = time.time() - start_time
                            print(f"📊 已接收 {line_count} 行，運行時間: {elapsed:.1f}秒")
                
                time.sleep(0.01)  # 短暫延遲
                
            except KeyboardInterrupt:
                print("\n🛑 用戶中斷監控")
                break
            except Exception as e:
                print(f"⚠️ 讀取錯誤: {e}")
                time.sleep(1)
                
    except serial.SerialException as e:
        print(f"❌ 串行端口錯誤: {e}")
        print("💡 建議:")
        print("   1. 檢查 ESP32 是否正確連接")
        print("   2. 確認端口號是否正確")
        print("   3. 嘗試其他端口")
          # 列出可用端口
        ports = serial.tools.list_ports.comports()
        if ports:
            print("\n📡 可用串行端口:")
            for port in ports:
                print(f"   - {port.device}: {port.description}")
    
    except Exception as e:
        print(f"❌ 未知錯誤: {e}")
    
    finally:
        try:
            ser.close()
            print("🔌 串行端口已關閉")
        except:
            pass

if __name__ == "__main__":
    # 檢查命令行參數
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = find_esp32_port()
        if not port:
            port = 'COM7'  # 默認端口
    
    monitor_serial(port)
