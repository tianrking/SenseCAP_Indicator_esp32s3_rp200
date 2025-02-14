arduino-cli compile --fqbn esp32:esp32:esp32s3 ./

arduino-cli config init

board_manager:
  additional_urls:
    - https://dl.espressif.com/dl/package_esp32_index.json

directories:
  data: /home/your_user/.arduino15
  downloads: /home/your_user/Downloads
  user: /home/your_user/Arduino

# Enable PSRAM settings for ESP32-S3 (OPI PSRAM)
core:
  enable_psram: true
  psram_type: opi
