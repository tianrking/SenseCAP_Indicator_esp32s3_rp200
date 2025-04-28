// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // <<< 用于事件同步
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"          // <<< 用于 NVS 初始化
#include "esp_wifi.h"           // <<< 用于 Wi-Fi
#include "esp_event.h"          // <<< 用于事件循环
#include "esp_netif.h"          // <<< 用于 TCP/IP 栈
#include "esp_sntp.h"           // <<< 用于 SNTP 时间同步
#include <time.h>               // <<< 用于时间函数

#include "bsp_board.h"
#include "lv_port.h"
#include "my_ui.h" // 包含你自定义 UI 的头文件

#define LOG_MEM_INFO        1 // 保留内存日志功能

// --- Wi-Fi 配置 ---
#define WIFI_SSID      "SEEED_Solution" // <<< 你的 Wi-Fi SSID
#define WIFI_PASSWORD  "chck1208"     // <<< 你的 Wi-Fi 密码
#define WIFI_MAXIMUM_RETRY  5         // <<< 最大重试次数

// --- 事件组位定义 ---
/* 用于等待 Wi-Fi 连接成功的位 */
#define WIFI_CONNECTED_BIT BIT0
/* 用于等待获取到 IP 地址的位 (通常与连接成功一起发生) */
#define WIFI_FAIL_BIT      BIT1
/* 用于等待 SNTP 同步完成的位 */
#define SNTP_TIME_SYNCED_BIT BIT2

// --- 全局变量 ---
static EventGroupHandle_t s_wifi_event_group; // Wi-Fi 事件组句柄
static int s_retry_num = 0;                   // Wi-Fi 重连计数器
static const char *TAG = "app_main";

// --- 函数声明 ---
static void initialize_wifi(void);
static void initialize_sntp(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void time_sync_notification_cb(struct timeval *tv);


/**
 * @brief Wi-Fi 事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // 开始连接
        ESP_LOGI(TAG, "Wi-Fi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect(); // 尝试重连
            s_retry_num++;
            ESP_LOGI(TAG, "Wi-Fi disconnected, retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); // 达到最大重试次数，设置失败位
            ESP_LOGE(TAG, "Connect to the AP fail after %d retries", WIFI_MAXIMUM_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // 连接成功，重置重试计数器
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); // 设置连接成功位
        // --- 连接成功后，初始化 SNTP ---
        initialize_sntp();
    }
}

/**
 * @brief SNTP 时间同步成功的回调函数
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP time synchronized");
    // 设置 SNTP 同步完成位
    xEventGroupSetBits(s_wifi_event_group, SNTP_TIME_SYNCED_BIT);

    // (可选) 设置系统时区为中国标准时间 (CST-8)
    setenv("TZ", "CST-8", 1);
    tzset(); // 应用时区设置

    // 打印一下当前时间确认
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time after sync: %s", strftime_buf);
}

/**
 * @brief 初始化 SNTP
 */
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    // 设置 SNTP 操作模式 (轮询)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // 设置时间服务器 (使用中国的 NTP 服务器)
    esp_sntp_setservername(0, "ntp.aliyun.com"); // 阿里云 NTP
    esp_sntp_setservername(1, "cn.pool.ntp.org"); // 中国 NTP 池
    esp_sntp_setservername(2, "edu.ntp.org.cn"); // 教育网 NTP (备用)
    // 设置时间同步成功的回调函数
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    // 初始化 SNTP
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized, waiting for time synchronization...");
}

/**
 * @brief 初始化 Wi-Fi 连接
 */
static void initialize_wifi(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    // 1. 创建事件组
    s_wifi_event_group = xEventGroupCreate();

    // 2. 初始化 TCP/IP 协议栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 3. 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 4. 创建默认的 Wi-Fi Station 网络接口
    esp_netif_create_default_wifi_sta();

    // 5. 初始化 Wi-Fi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 6. 注册事件处理函数
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, // 复用 wifi_event_handler 处理 IP 事件
                                                        NULL,
                                                        &instance_got_ip));

    // 7. 配置 Wi-Fi Station 模式
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 推荐使用 WPA2
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // 8. 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "Wi-Fi initialization finished.");

    /* 等待连接成功或者连接失败 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, // 不需要清除事件位
            pdFALSE, // 等待任一事件位
            portMAX_DELAY); // 一直等待

    /* 根据等待结果进行处理 */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        // 这里可以添加失败处理逻辑，例如重启或进入错误状态
    } else {
        ESP_LOGE(TAG, "UNEXPECTED WIFI EVENT");
    }

    /* 事件处理函数不再需要，注销 (可选，如果后面不再需要处理 Wi-Fi 事件) */
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    // vEventGroupDelete(s_wifi_event_group); // 如果后面完全不用事件组了可以删除
}

void app_main(void)
{
    ESP_LOGI(TAG, "System start");

    // --- 1. 初始化 NVS ---
    // 非易失性存储是 Wi-Fi 配置等所必需的
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // --- 2. 初始化 Wi-Fi 并连接 ---
    initialize_wifi();

    // --- 3. 等待 SNTP 时间同步完成 ---
    // (initialize_sntp 在 Wi-Fi 连接成功后被调用)
    ESP_LOGI(TAG, "Waiting for SNTP time synchronization...");
    EventBits_t sntp_bits = xEventGroupWaitBits(s_wifi_event_group,
                                                SNTP_TIME_SYNCED_BIT,
                                                pdTRUE,  // 同步成功后清除标志位
                                                pdFALSE, // 等待 SNTP_TIME_SYNCED_BIT 被设置
                                                pdMS_TO_TICKS(15000)); // 设置超时时间 (例如 15 秒)

    if (sntp_bits & SNTP_TIME_SYNCED_BIT) {
        ESP_LOGI(TAG, "SNTP synchronization successful.");
    } else {
        ESP_LOGW(TAG, "SNTP synchronization timed out or failed. Time might be incorrect.");
        // 即使同步失败，也继续执行，但时间可能不准
    }

    // --- 4. 初始化硬件和 LVGL ---
    ESP_LOGI(TAG, "Initializing Board and LVGL...");
    ESP_ERROR_CHECK(bsp_board_init());
    lv_port_init();

#if CONFIG_LCD_AVOID_TEAR
    ESP_LOGI(TAG, "Avoid lcd tearing effect");
#if CONFIG_LCD_LVGL_FULL_REFRESH
    ESP_LOGI(TAG, "LVGL full-refresh");
#elif CONFIG_LCD_LVGL_DIRECT_MODE
    ESP_LOGI(TAG, "LVGL direct-mode");
#endif
#endif

    // --- 5. 初始化你的 UI ---
    ESP_LOGI(TAG, "Initializing Custom UI...");
    lv_port_sem_take(); // 获取 LVGL 信号量
    my_ui_heatmap_init(); // 调用你的 UI 初始化函数
    lv_port_sem_give(); // 释放 LVGL 信号量
    ESP_LOGI(TAG, "Custom UI Initialized.");


    // --- 6. 内存日志循环 (如果启用) ---
#if LOG_MEM_INFO
    static char buffer[128];
    while (1) {
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  DRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM", "%s", buffer);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif
}
