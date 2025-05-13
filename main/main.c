// main.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "bsp_board.h"
#include "lv_port.h"
#include "my_ui.h" // my_ui.h 中包含 extern volatile bool g_wifi_is_connected;

#define LOG_MEM_INFO 1

static const char *TAG = "app_main";

// WiFi 配置
#define WIFI_SSID      "SEEED_Solution" // 请确保这是您要连接的SSID
#define WIFI_PASSWORD  "chck1208"     // 请确保这是正确的密码

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
#define WIFI_MAXIMUM_RETRY  5

// 全局 WiFi 连接状态标志
volatile bool g_wifi_is_connected = false; // <<< 定义全局变量

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Wi-Fi station mode started, trying to connect...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_is_connected = false; 
        ESP_LOGW(TAG, "WiFi Disconnected");
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connect to the AP failed after %d retries", s_retry_num);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        g_wifi_is_connected = true; 
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Disabling WiFi Modem Sleep (WIFI_PS_NONE)");
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "wifi_init_sta finished. Waiting for connection...");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
        g_wifi_is_connected = true; 
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        g_wifi_is_connected = false;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED WIFI EVENT");
        g_wifi_is_connected = false;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "System start");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();

    if (!g_wifi_is_connected) {
        ESP_LOGE(TAG, "WiFi not connected post init! Map functionality will be severely limited or non-functional.");
    }

    ESP_LOGI(TAG, "Initializing board and LVGL port...");
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

    lv_port_sem_take();
    // my_ui_init();
     lv_png_init();
    my_ui_init_single_tile();
    lv_port_sem_give();

#if LOG_MEM_INFO
    static char buffer[128];
    while (1) {
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  DRAM : [%8zu / %8zu / %8zu]\n"
                "\t PSRAM : [%8zu / %8zu / %8zu]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM", "%s", buffer);
        ESP_LOGI(TAG, "Current WiFi Connection Status: %s", g_wifi_is_connected ? "CONNECTED" : "DISCONNECTED");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif
}