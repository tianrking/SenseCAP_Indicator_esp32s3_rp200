#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "nubia";       // 你的 Wi-Fi SSID
const char* password = "22222222"; // 你的 Wi-Fi 密码

WebServer server(80); // Web server object on port 80

// 处理根路径 ("/") 的 GET 请求
void handleRoot() {
  server.send(200, "text/plain", "Hello from ESP32-S3 Web Server!");
}

// 处理 POST 请求 (任何路径)
void handlePost() {
  Serial.println("Received POST request:");

  // 打印请求的路径
  Serial.print("  Path: ");
  Serial.println(server.uri());

  // 打印请求头
  Serial.println("  Headers:");
  for (int i = 0; i < server.headers(); i++) {
    Serial.print("    ");
    Serial.print(server.headerName(i));
    Serial.print(": ");
    Serial.println(server.header(i));
  }

  // 打印请求参数 (如果有)
  Serial.println("  Arguments:");
  for (int i = 0; i < server.args(); i++) {
    Serial.print("    ");
    Serial.print(server.argName(i));
    Serial.print(": ");
    Serial.println(server.arg(i));
  }

  // 打印请求体 (如果有)
    if (server.hasArg("plain")) {
      Serial.println("  Body:");
      Serial.print("   ");
      Serial.println(server.arg("plain"));
    }

  // 打印上传的文件信息 (如果有)
    if(server.hasArg("file")){
      Serial.println(server.arg("file"));
    }


  server.send(200, "text/plain", "POST request received.");
}

void handleNotFound() {
    server.send(404, "text/plain", "404 Not Found");
}

void setup() {
  Serial.begin(115200);

  // 连接 Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 设置路由
  server.on("/", HTTP_GET, handleRoot);        // 处理 GET 请求到根路径
  server.onNotFound(handleNotFound);          // 404
  server.on("/upload", HTTP_POST, []() {      // 处理所有其他的 POST 请求
      server.send(200);
    }, handlePost);


  // 启动 Web 服务器
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient(); // 处理客户端请求
  delay(1); // 短暂延时
}