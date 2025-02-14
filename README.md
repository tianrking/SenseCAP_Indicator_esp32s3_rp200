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

curl -F "file=@your_image.jpg" http://<your_esp32_ip>/upload

curl -F "file=@aa.jpg" http://192.168.238.18/upload
curl -F "file=@your_image.jpg" http://<your_esp32_ip>/upload

curl -X POST -d "param1=value1¶m2=value2" http://192.168.238.18/upload

curl -X POST -H "Content-Type: application/json" -d '{"key1":"value1", "key2":123}' http://192.168.238.18/upload