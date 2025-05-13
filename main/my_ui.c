// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_port.h"
#include "lodepng.h"

static const char *TAG = "my_ui_lodepng_tile";

static lv_obj_t *single_tile_img_obj;
static lv_obj_t *status_label_simple;

typedef struct {
    bool is_http_transfer_active; // 用于事件处理器，标记HTTP传输是否仍在进行
    bool has_decoded_data;
    lv_img_dsc_t img_dsc;
    uint8_t *downloaded_png_buf;
    size_t downloaded_png_buf_size;
    uint8_t *decoded_rgb565_buf;
} single_tile_info_t;

static single_tile_info_t current_tile_data;

static void download_and_decode_tile_task(void *pvParameters);
static void cleanup_tile_buffers(void);

esp_err_t _http_event_handler_simple(esp_http_client_event_t *evt) {
    static int output_len_simple;
    single_tile_info_t *tile_info = (single_tile_info_t *)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            if (tile_info) {
                tile_info->is_http_transfer_active = false;
                if (tile_info->downloaded_png_buf) {
                    ESP_LOGW(TAG, "Freeing downloaded_png_buf due to HTTP_EVENT_ERROR.");
                    free(tile_info->downloaded_png_buf);
                    tile_info->downloaded_png_buf = NULL;
                }
                tile_info->downloaded_png_buf_size = 0; // 确保大小也清零
            }
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            output_len_simple = 0;
            if (tile_info) {
                // 此处不应有 downloaded_png_buf，因为它应在任务开始时被 cleanup_tile_buffers 清理
                if (tile_info->downloaded_png_buf) { 
                    ESP_LOGW(TAG, "downloaded_png_buf was not NULL in ON_CONNECTED! Freeing. Size: %u", (unsigned int)tile_info->downloaded_png_buf_size);
                    free(tile_info->downloaded_png_buf);
                    tile_info->downloaded_png_buf = NULL;
                }
                tile_info->downloaded_png_buf_size = 0;
                tile_info->is_http_transfer_active = true; // HTTP传输开始
            }
            break;
        // ... HTTP_EVENT_HEADERS_SENT, HTTP_EVENT_ON_HEADER 保持不变 ...
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // ... (与上一版相同，只是使用 downloaded_png_buf) ...
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!tile_info) {
                ESP_LOGE(TAG, "tile_info is NULL in ON_DATA");
                return ESP_FAIL;
            }
            if (evt->data_len == 0) break;

            if (tile_info->downloaded_png_buf == NULL) {
                size_t initial_alloc_size = 16 * 1024;
                long long content_length_ll = esp_http_client_get_content_length(evt->client);
                ESP_LOGD(TAG, "Content-Length: %lld", content_length_ll);

                if (content_length_ll > 0 && content_length_ll < (150 * 1024)) {
                    initial_alloc_size = (size_t)content_length_ll;
                } else if (content_length_ll >= (150 * 1024)) {
                     ESP_LOGE(TAG, "Content-Length too large for tile: %lld. Aborting.", content_length_ll);
                     return ESP_FAIL;
                }
                ESP_LOGD(TAG, "Allocating initial downloaded_png_buf of size: %u", (unsigned int)initial_alloc_size);
                tile_info->downloaded_png_buf = (uint8_t *)malloc(initial_alloc_size);
                if (!tile_info->downloaded_png_buf) {
                    ESP_LOGE(TAG, "Failed to allocate initial downloaded_png_buf (size: %u)", (unsigned int)initial_alloc_size);
                    return ESP_FAIL;
                }
                tile_info->downloaded_png_buf_size = initial_alloc_size;
                output_len_simple = 0;
            }

            if (output_len_simple + evt->data_len > tile_info->downloaded_png_buf_size) {
                if (tile_info->downloaded_png_buf_size >= (150 * 1024)) {
                     ESP_LOGE(TAG, "Tile exceeded max buffer size (150KB) during realloc, discarding.");
                    free(tile_info->downloaded_png_buf);
                    tile_info->downloaded_png_buf = NULL; tile_info->downloaded_png_buf_size = 0;
                    return ESP_FAIL;
                }
                size_t new_size = tile_info->downloaded_png_buf_size + 16 * 1024;
                if (new_size < output_len_simple + evt->data_len) new_size = output_len_simple + evt->data_len;
                if (new_size > (150*1024)) new_size = (150*1024);
                
                ESP_LOGD(TAG, "Reallocating downloaded_png_buf from %u to %u", (unsigned int)tile_info->downloaded_png_buf_size, (unsigned int)new_size);
                uint8_t *temp_buf = (uint8_t *)realloc(tile_info->downloaded_png_buf, new_size);
                if (temp_buf == NULL) {
                    ESP_LOGE(TAG, "Failed to realloc downloaded_png_buf");
                    free(tile_info->downloaded_png_buf); tile_info->downloaded_png_buf = NULL; tile_info->downloaded_png_buf_size = 0;
                    return ESP_FAIL;
                }
                tile_info->downloaded_png_buf = temp_buf;
                tile_info->downloaded_png_buf_size = new_size;
            }
            memcpy(tile_info->downloaded_png_buf + output_len_simple, evt->data, evt->data_len);
            output_len_simple += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH, total PNG data received: %d", output_len_simple);
            if (tile_info) {
                tile_info->is_http_transfer_active = false; // HTTP传输完成
                if (output_len_simple > 0 && tile_info->downloaded_png_buf) {
                     if (output_len_simple < tile_info->downloaded_png_buf_size) {
                        ESP_LOGD(TAG, "Shrinking downloaded_png_buf from %u to %d", (unsigned int)tile_info->downloaded_png_buf_size, output_len_simple);
                        uint8_t *final_buf = realloc(tile_info->downloaded_png_buf, output_len_simple);
                        if (final_buf) {
                            tile_info->downloaded_png_buf = final_buf;
                            tile_info->downloaded_png_buf_size = output_len_simple;
                        } else {
                            ESP_LOGW(TAG, "Failed to shrink downloaded_png_buf. Actual data size: %d", output_len_simple);
                             // 保持较大的缓冲区，但记录实际大小
                            tile_info->downloaded_png_buf_size = output_len_simple;
                        }
                    } else {
                         tile_info->downloaded_png_buf_size = output_len_simple;
                    }
                    ESP_LOGI(TAG, "PNG data download complete and buffer finalized by ON_FINISH. Size: %u", (unsigned int)tile_info->downloaded_png_buf_size);
                } else { // output_len_simple is 0 or buffer is NULL
                    ESP_LOGW(TAG, "HTTP_EVENT_ON_FINISH but no PNG data or buffer. output_len: %d", output_len_simple);
                    if (tile_info->downloaded_png_buf) { // defensive free
                        free(tile_info->downloaded_png_buf);
                        tile_info->downloaded_png_buf = NULL;
                    }
                    tile_info->downloaded_png_buf_size = 0;
                }
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (tile_info) {
                // 如果 is_http_transfer_active 仍为 true，说明在 ON_FINISH 或 ON_ERROR 之前断开
                if (tile_info->is_http_transfer_active) { 
                    ESP_LOGW(TAG, "HTTP disconnected unexpectedly while transfer active. Cleaning up any partial PNG buffer.");
                    if (tile_info->downloaded_png_buf) {
                        free(tile_info->downloaded_png_buf);
                        tile_info->downloaded_png_buf = NULL;
                    }
                    tile_info->downloaded_png_buf_size = 0;
                }
                tile_info->is_http_transfer_active = false; // 确保标记为非活动
            }
            break;
        default:
            ESP_LOGD(TAG, "Unhandled HTTP event: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

static esp_err_t convert_rgba_to_rgb565(const unsigned char *rgba_buf, uint16_t *rgb565_buf, unsigned int width, unsigned int height) {
    // ... (与上一版相同) ...
    if (!rgba_buf || !rgb565_buf) {
        ESP_LOGE(TAG, "convert_rgba_to_rgb565: NULL input buffer(s)");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Starting RGBA to RGB565 conversion for %ux%u image", width, height);
    for (unsigned int i = 0; i < width * height; ++i) {
        unsigned char r = rgba_buf[i * 4 + 0];
        unsigned char g = rgba_buf[i * 4 + 1];
        unsigned char b = rgba_buf[i * 4 + 2];
        rgb565_buf[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    ESP_LOGI(TAG, "RGBA to RGB565 conversion finished.");
    return ESP_OK;
}

static void download_and_decode_tile_task(void *pvParameters) {
    if (!g_wifi_is_connected) {
        ESP_LOGE(TAG, "WiFi not connected.");
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
             lv_port_sem_take(); lv_label_set_text(status_label_simple, "WiFi disconnected"); lv_port_sem_give();
        }
        vTaskDelete(NULL); return;
    }

    ESP_LOGI(TAG, "Task: Download tile X:%d,Y:%d,Z:%d", FIXED_TILE_X, FIXED_TILE_Y, FIXED_TILE_Z);
    if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
        lv_port_sem_take(); lv_label_set_text(status_label_simple, "Downloading..."); lv_port_sem_give();
    }

    cleanup_tile_buffers(); // 清理上一次的所有相关缓冲区
    current_tile_data.is_http_transfer_active = true; // 为事件处理器设置初始状态
    current_tile_data.has_decoded_data = false;

    char url_buffer[256];
    sprintf(url_buffer, TILE_URL_FORMAT, FIXED_TILE_X, FIXED_TILE_Y, FIXED_TILE_Z);

    esp_http_client_config_t config = { /* ... 与上一版相同 ... */
        .url = url_buffer,
        .event_handler = _http_event_handler_simple,
        .user_data = &current_tile_data,
        .timeout_ms = 20000,
        .buffer_size = 2048,
        .buffer_size_tx = 512,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // ... (client 初始化失败检查与上一版相同) ...
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_port_sem_take(); lv_label_set_text(status_label_simple, "HTTP client error"); lv_port_sem_give();
        }
        cleanup_tile_buffers(); 
        vTaskDelete(NULL); return;
    }

    esp_err_t http_perform_err = esp_http_client_perform(client);
    // perform 返回后，is_http_transfer_active 应已被事件处理器设为false
    
    bool actual_download_success = (http_perform_err == ESP_OK &&
                                   current_tile_data.downloaded_png_buf != NULL &&
                                   current_tile_data.downloaded_png_buf_size > 0);

    if (actual_download_success) {
        ESP_LOGI(TAG, "HTTP Perform OK. PNG data size: %u. Starting LodePNG decoding.", (unsigned int)current_tile_data.downloaded_png_buf_size);
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_port_sem_take(); lv_label_set_text(status_label_simple, "Decoding..."); lv_port_sem_give();
        }

        unsigned char* temp_rgba_buf = NULL;
        unsigned int png_width = 0, png_height = 0;
        
        ESP_LOGI(TAG, "Heap before LodePNG: %d", (int)esp_get_free_heap_size());
        unsigned int lodepng_err = lodepng_decode32(&temp_rgba_buf, &png_width, &png_height,
                                            current_tile_data.downloaded_png_buf,
                                            current_tile_data.downloaded_png_buf_size);
        ESP_LOGI(TAG, "Heap after LodePNG (temp_rgba_buf at %p): %d", (void*)temp_rgba_buf, (int)esp_get_free_heap_size());

        // PNG数据已被LodePNG处理（或尝试处理），可以释放了
        if(current_tile_data.downloaded_png_buf) {
            free(current_tile_data.downloaded_png_buf);
            current_tile_data.downloaded_png_buf = NULL;
            current_tile_data.downloaded_png_buf_size = 0;
        }

        if (lodepng_err) {
            ESP_LOGE(TAG, "LodePNG error %u: %s", lodepng_err, lodepng_error_text(lodepng_err));
            current_tile_data.has_decoded_data = false;
        } else {
            ESP_LOGI(TAG, "LodePNG decoded to RGBA: %ux%u", png_width, png_height);
            if (png_width == TILE_SIZE && png_height == TILE_SIZE) {
                current_tile_data.decoded_rgb565_buf = (uint8_t*)malloc(png_width * png_height * sizeof(uint16_t));
                if (!current_tile_data.decoded_rgb565_buf) {
                    ESP_LOGE(TAG, "Failed to alloc for RGB565 data");
                    current_tile_data.has_decoded_data = false;
                } else {
                    if (convert_rgba_to_rgb565(temp_rgba_buf, (uint16_t*)current_tile_data.decoded_rgb565_buf, png_width, png_height) == ESP_OK) {
                        current_tile_data.img_dsc.header.w = png_width;
                        current_tile_data.img_dsc.header.h = png_height;
                        current_tile_data.img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
                        current_tile_data.img_dsc.data = current_tile_data.decoded_rgb565_buf;
                        current_tile_data.img_dsc.data_size = png_width * png_height * sizeof(uint16_t);
                        current_tile_data.has_decoded_data = true;
                        ESP_LOGI(TAG, "Image ready as RGB565.");
                    } else {
                        ESP_LOGE(TAG, "RGBA to RGB565 conversion failed.");
                        free(current_tile_data.decoded_rgb565_buf);
                        current_tile_data.decoded_rgb565_buf = NULL;
                        current_tile_data.has_decoded_data = false;
                    }
                }
                free(temp_rgba_buf); // 释放LodePNG分配的临时RGBA缓冲区
            } else {
                ESP_LOGE(TAG, "Decoded PNG dim (%ux%u) != TILE_SIZE (%d)", png_width, png_height, TILE_SIZE);
                free(temp_rgba_buf);
                current_tile_data.has_decoded_data = false;
            }
        }
    } else { // download_ok is false
        ESP_LOGE(TAG, "Download failed or no data. HTTP Err: %s, buf: %p, size: %u", 
                 esp_err_to_name(http_perform_err), 
                 (void*)current_tile_data.downloaded_png_buf, 
                 (unsigned int)current_tile_data.downloaded_png_buf_size);
        cleanup_tile_buffers(); // 确保任何可能残留的缓冲区被清理
    }
    
    esp_http_client_cleanup(client); // Cleanup HTTP client regardless of success/failure

    // --- LVGL UI Update ---
    // ... (与上一版相同，基于 current_tile_data.has_decoded_data) ...
    if (single_tile_img_obj && lv_obj_is_valid(single_tile_img_obj)) {
        lv_port_sem_take();
        if (current_tile_data.has_decoded_data && current_tile_data.decoded_rgb565_buf != NULL) {
            ESP_LOGI(TAG, "Displaying decoded RGB565. Ptr: %p, Size: %u",
                     (void*)current_tile_data.img_dsc.data, (unsigned int)current_tile_data.img_dsc.data_size);
            
            lv_img_set_src(single_tile_img_obj, &current_tile_data.img_dsc);
            lv_obj_clear_flag(single_tile_img_obj, LV_OBJ_FLAG_HIDDEN);
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                 lv_label_set_text_fmt(status_label_simple, "Tile RGB565 (%ux%u)\nX:%d Y:%d Z:%d",
                                      current_tile_data.img_dsc.header.w, current_tile_data.img_dsc.header.h,
                                      FIXED_TILE_X, FIXED_TILE_Y, FIXED_TILE_Z);
            }
            ESP_LOGI(TAG, "Tile display command sent (RGB565).");
        } else {
            ESP_LOGE(TAG, "No valid decoded data to display.");
            lv_obj_add_flag(single_tile_img_obj, LV_OBJ_FLAG_HIDDEN);
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                 lv_label_set_text(status_label_simple, "Failed: No image data.");
            }
        }
        lv_port_sem_give();
    } else {
         ESP_LOGE(TAG, "single_tile_img_obj is NULL or invalid, cannot update UI.");
    }
    vTaskDelete(NULL);
}

static void cleanup_tile_buffers(void) {
    ESP_LOGD(TAG, "cleanup_tile_buffers called.");
    if (current_tile_data.downloaded_png_buf) {
        ESP_LOGD(TAG, "Freeing downloaded_png_buf in cleanup. Addr: %p", (void*)current_tile_data.downloaded_png_buf);
        free(current_tile_data.downloaded_png_buf);
        current_tile_data.downloaded_png_buf = NULL;
    }
    current_tile_data.downloaded_png_buf_size = 0;

    if (current_tile_data.decoded_rgb565_buf) {
        ESP_LOGD(TAG, "Freeing decoded_rgb565_buf in cleanup. Addr: %p", (void*)current_tile_data.decoded_rgb565_buf);
        free(current_tile_data.decoded_rgb565_buf);
        current_tile_data.decoded_rgb565_buf = NULL;
    }
    
    current_tile_data.has_decoded_data = false;
    // current_tile_data.is_http_transfer_active should be managed by the http events themselves mostly
    memset(&current_tile_data.img_dsc, 0, sizeof(lv_img_dsc_t));
}

void my_ui_init_single_tile(void) {
    // ... (与上一版相同，确保任务名是 download_and_decode_tile_task) ...
    ESP_LOGI(TAG, "Initializing UI for LodePNG dynamic tile...");

    lv_obj_t *scr = lv_scr_act();
    if (!scr) {
        ESP_LOGE(TAG, "Failed to get active screen!");
        return;
    }
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    memset(&current_tile_data, 0, sizeof(single_tile_info_t)); 

    single_tile_img_obj = lv_img_create(scr);
    if (!single_tile_img_obj) {
        ESP_LOGE(TAG, "Failed to create single_tile_img_obj!");
        return;
    }
    lv_obj_set_pos(single_tile_img_obj, (SCREEN_WIDTH - TILE_SIZE) / 2, (SCREEN_HEIGHT - TILE_SIZE) / 2);
    lv_obj_set_size(single_tile_img_obj, TILE_SIZE, TILE_SIZE); 
    lv_obj_add_flag(single_tile_img_obj, LV_OBJ_FLAG_HIDDEN);

    status_label_simple = lv_label_create(scr);
    if (!status_label_simple) {
        ESP_LOGE(TAG, "Failed to create status_label_simple!");
    } else {
        lv_obj_align(status_label_simple, LV_ALIGN_BOTTOM_LEFT, 5, -5);
        lv_label_set_text(status_label_simple, "Waiting for WiFi...");
        lv_obj_set_style_bg_opa(status_label_simple, LV_OPA_70, 0);
        lv_obj_set_style_bg_color(status_label_simple, lv_color_black(), 0);
        lv_obj_set_style_text_color(status_label_simple, lv_color_white(), 0);
        lv_obj_set_style_pad_all(status_label_simple, 3, 0);
    }

    if (g_wifi_is_connected) {
        ESP_LOGI(TAG, "WiFi connected, creating download & decode task.");
        BaseType_t task_created = xTaskCreate(download_and_decode_tile_task, "tile_dl_decode", 1024 * 8, NULL, 5, NULL); // 栈大小增加到8KB
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create download_and_decode_tile_task!");
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                lv_label_set_text(status_label_simple, "Task creation failed!");
            }
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected at UI init.");
         if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_label_set_text(status_label_simple, "Connect to WiFi...");
        }
    }
}