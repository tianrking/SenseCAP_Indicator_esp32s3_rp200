import serial
import time
import json # To convert Python dictionaries to JSON strings

# --- 配置区 ---
SERIAL_PORT = '/dev/ttyUSB0'  # Linux 示例, Windows上可能是 'COM3', macOS上可能是 '/dev/tty.usbserial-XXXX'
BAUD_RATE = 115200
DELAY_BETWEEN_SENDS_SECONDS = 10 # 等待ESP32处理和您观察的时间

# --- JSON 测试数据定义 (作为 Python 字典) ---

json_test_case_1 = {
    "screen_id": "welcome_screen_py",
    "background_color": "#F0F0F0",
    "layout": {
        "type": "column",
        "padding": 20,
        "gap": 25,
        "item_alignment": "center_horizontal",
        "distribution": "center"
    },
    "elements": [
        {
            "type": "label",
            "id": "lbl_title_py1",
            "text": "Python Test 1: Welcome!",
            "style_ref": "theme.font.large_title",
            "text_color": "#333333"
        },
        {
            "type": "label",
            "id": "lbl_subtitle_py1",
            "text": "Sent via Python Script",
            "style_ref": "theme.font.normal_text",
            "text_color": "#555555"
        },
        {
            "type": "button",
            "id": "btn_next1_py",
            "label": "Next UI (Py)",
            "style_ref": "theme.button.primary",
            "width": 180 # Adjusted width for longer text
        }
    ]
}

json_test_case_2 = {
    "screen_id": "info_screen_py",
    "background_color": "#2C3E50",
    "layout": {
        "type": "column",
        "padding": 15,
        "gap": 20,
        "item_alignment": "center_horizontal"
    },
    "elements": [
        {
            "type": "label",
            "id": "header_title_py2",
            "text": "Python Test 2: Info Card",
            "style_ref": "theme.font.large_title",
            "text_color": "#ECF0F1"
        },
        {
            "type": "container",
            "id": "info_card_py2",
            "style_ref": "theme.card",
            "width": "300px",
            "padding": "15px",
            "layout": {
                "type": "column",
                "gap": 10
            },
            "elements": [
                {
                    "type": "label",
                    "text": "System Status:",
                    "style_ref": "theme.font.normal_text",
                    "text_color": "#2C3E50"
                },
                {
                    "type": "container",
                    "layout": {
                        "type": "row",
                        "gap": 10,
                        "distribution": "space_between"
                    },
                    "width": "100%", # Relative to parent card
                    "elements": [
                        {"type": "label", "text": "Sensor A:", "style_ref": "theme.font.normal_text"},
                        {"type": "label", "text": "Online", "style_ref": "theme.font.normal_text", "text_color": "#27AE60"}
                    ]
                },
                {
                    "type": "container",
                    "layout": {
                        "type": "row",
                        "gap": 10,
                        "distribution": "space_between"
                    },
                    "width": "100%",
                     "elements": [
                        {"type": "label", "text": "Mode:", "style_ref": "theme.font.normal_text"},
                        {"type": "label", "text": "Automatic", "style_ref": "theme.font.normal_text", "text_color": "#3498DB"}
                    ]
                }
            ]
        },
        {
            "type": "button",
            "id": "btn_next2_py",
            "label": "Next UI (Py)",
            "style_ref": "theme.button.danger",
            "width": "80%" # Relative to parent layout constraints
        }
    ]
}

json_test_case_3 = {
    "screen_id": "nested_layout_py",
    "background_color": "#BDC3C7", # A neutral background
    "layout": {
        "type": "column",
        "padding": 10,
        "gap": 10
    },
    "elements": [
        {
            "type": "container",
            "id": "header_bar_py3",
            "background_color": "#2980B9", # A nice blue
            "padding": "10px",
            "width": "100%", # Full width of screen
            "height": 50,
            "layout": {
                "type": "column",
                "item_alignment": "center_horizontal",
                "distribution": "center"
            },
            "elements": [
                {
                    "type": "label",
                    "text": "Python Test 3: Nested & Fill",
                    "style_ref": "theme.font.normal_text", # Using normal_text as large_title might be too big for height 50
                    "text_color": "#FFFFFF"
                }
            ]
        },
        {
            "type": "container",
            "id": "main_content_area_py3",
             # For "fill_horizontal" with workaround, children need width:"100%"
            "layout": { 
                "type": "column",
                "gap": 8,
                "padding": "5px",
                "item_alignment": "center_horizontal" # Center non-100% children
            },
            "width": "100%", # Main content area also full width
            "background_color": "#ECF0F1", # Light background for content
            "elements": [
                {
                    "type": "label",
                    "text": "Label above stretched container."
                },
                {
                    "type": "container",
                    "id": "stretched_child_py3",
                    "background_color": "#7F8C8D", # Mid-grey
                    "padding": "10px",
                    "width": "100%", # This child will stretch to fill main_content_area_py3
                    "height": 80,
                    "layout":{"type":"column", "distribution":"center", "item_alignment":"center_horizontal"},
                    "elements": [
                        {"type": "label", "text": "This container is 100% wide.", "text_color":"#FFFFFF"}
                    ]
                },
                 {
                    "type": "label",
                    "text": "Label below stretched container."
                }
            ]
        },
        {
            "type": "button",
            "id": "btn_finish_py",
            "label": "Test Cycle Done",
            "style_ref": "theme.button.primary",
            "width": 200, # Fixed width
            "height": 40
        }
    ]
}

# 列表包含所有测试用例
test_cases = [
    {"name": "Test Case 1: Welcome Screen", "data": json_test_case_1},
    {"name": "Test Case 2: Info Card Screen", "data": json_test_case_2},
    {"name": "Test Case 3: Nested Layout Screen", "data": json_test_case_3}
]

def send_json_over_serial(ser, json_data_dict, case_name):
    """将Python字典转换为JSON字符串并发送"""
    try:
        # 将字典转换为紧凑的单行JSON字符串
        json_string = json.dumps(json_data_dict, separators=(',', ':'))
        
        print(f"\n--- Sending {case_name} ---")
        print(f"JSON String: {json_string}")
        
        # 编码为字节串并添加换行符
        data_to_send = json_string.encode('utf-8') + b'\n'
        
        ser.write(data_to_send)
        print(f"Sent {len(data_to_send)} bytes to {ser.name}")
        
        # (可选) 等待来自ESP32的简单响应
        # time.sleep(0.1) # 给ESP32一点时间响应
        # if ser.in_waiting > 0:
        #     response = ser.readline().decode('utf-8').strip()
        #     print(f"ESP32 Response: {response}")
            
    except Exception as e:
        print(f"Error sending JSON for {case_name}: {e}")

if __name__ == "__main__":
    print(f"Attempting to open serial port: {SERIAL_PORT} at {BAUD_RATE} baud.")
    
    try:
        # with 语句确保串口在使用后被关闭
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            print(f"Serial port {ser.name} opened successfully.")
            time.sleep(2) # 等待串口稳定和ESP32准备就绪

            for i, case in enumerate(test_cases):
                send_json_over_serial(ser, case["data"], case["name"])
                if i < len(test_cases) - 1: # 如果不是最后一个测试用例
                    print(f"Waiting {DELAY_BETWEEN_SENDS_SECONDS} seconds before next send...")
                    time.sleep(DELAY_BETWEEN_SENDS_SECONDS)
            
            print("\n--- All test cases sent. ---")

    except serial.SerialException as e:
        print(f"Error opening or using serial port {SERIAL_PORT}: {e}")
        print("Please check the following:")
        print(f"1. Is the ESP32 connected to '{SERIAL_PORT}'?")
        print(f"2. Is the port correct? (e.g., COMx on Windows, /dev/ttyUSBx or /dev/ttyACMx on Linux, /dev/tty.usbserial-xxxx on macOS)")
        print("3. Do you have the necessary permissions to access the serial port?")
        print("4. Is another program (like idf.py monitor or another serial terminal) already using the port?")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

    print("Script finished.")
