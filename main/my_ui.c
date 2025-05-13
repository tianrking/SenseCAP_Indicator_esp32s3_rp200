// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h" // Kept for other esp_http_client functions if needed, but not for crt_bundle_attach
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Added for mutex
#include "lv_port.h"
#include "lodepng.h"

static const char *TAG = "my_ui_lodepng_tile";

static lv_obj_t *single_tile_img_obj;
static lv_obj_t *status_label_simple;

typedef struct {
    bool is_http_transfer_active;
    bool has_decoded_data;
    lv_img_dsc_t img_dsc;
    uint8_t *downloaded_png_buf;
    size_t downloaded_png_buf_size;
    uint8_t *decoded_rgb565_buf;
} single_tile_info_t;

static single_tile_info_t current_tile_data;

// --- MODIFICATION START: Global tile coordinates and control flags ---
static int g_current_tile_x;
static int g_current_tile_y;
static int g_current_tile_z;
static volatile bool g_is_tile_task_running = false;
static SemaphoreHandle_t g_tile_op_mutex = NULL;
// --- MODIFICATION END ---

static void download_and_decode_tile_task(void *pvParameters);
static void cleanup_tile_buffers(void);
static void screen_swipe_event_cb(lv_event_t *e); // Forward declaration for swipe handler


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
                tile_info->downloaded_png_buf_size = 0;
            }
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            output_len_simple = 0;
            if (tile_info) {
                if (tile_info->downloaded_png_buf) {
                    ESP_LOGW(TAG, "downloaded_png_buf was not NULL in ON_CONNECTED! Freeing. Size: %u", (unsigned int)tile_info->downloaded_png_buf_size);
                    free(tile_info->downloaded_png_buf);
                    tile_info->downloaded_png_buf = NULL;
                }
                tile_info->downloaded_png_buf_size = 0;
                tile_info->is_http_transfer_active = true;
            }
            break;
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
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
                tile_info->is_http_transfer_active = false;
                if (output_len_simple > 0 && tile_info->downloaded_png_buf) {
                    if (output_len_simple < tile_info->downloaded_png_buf_size) {
                        ESP_LOGD(TAG, "Shrinking downloaded_png_buf from %u to %d", (unsigned int)tile_info->downloaded_png_buf_size, output_len_simple);
                        uint8_t *final_buf = realloc(tile_info->downloaded_png_buf, output_len_simple);
                        if (final_buf) {
                            tile_info->downloaded_png_buf = final_buf;
                            tile_info->downloaded_png_buf_size = output_len_simple;
                        } else {
                            ESP_LOGW(TAG, "Failed to shrink downloaded_png_buf. Actual data size: %d", output_len_simple);
                            tile_info->downloaded_png_buf_size = output_len_simple;
                        }
                    } else {
                         tile_info->downloaded_png_buf_size = output_len_simple;
                    }
                    ESP_LOGI(TAG, "PNG data download complete and buffer finalized by ON_FINISH. Size: %u", (unsigned int)tile_info->downloaded_png_buf_size);
                } else {
                    ESP_LOGW(TAG, "HTTP_EVENT_ON_FINISH but no PNG data or buffer. output_len: %d", output_len_simple);
                    if (tile_info->downloaded_png_buf) {
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
                if (tile_info->is_http_transfer_active) {
                    ESP_LOGW(TAG, "HTTP disconnected unexpectedly while transfer active. Cleaning up any partial PNG buffer.");
                    if (tile_info->downloaded_png_buf) {
                        free(tile_info->downloaded_png_buf);
                        tile_info->downloaded_png_buf = NULL;
                    }
                    tile_info->downloaded_png_buf_size = 0;
                }
                tile_info->is_http_transfer_active = false;
            }
            break;
        default:
            ESP_LOGD(TAG, "Unhandled HTTP event: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

static esp_err_t convert_rgba_to_rgb565(const unsigned char *rgba_buf, uint16_t *rgb565_buf, unsigned int width, unsigned int height) {
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
    bool original_task_should_run_main_logic = true; // Renamed for clarity within this specific task instance

    if (!g_wifi_is_connected) {
        ESP_LOGE(TAG, "WiFi not connected for task X%d Y%d.", g_current_tile_x, g_current_tile_y);
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_port_sem_take(); lv_label_set_text(status_label_simple, "WiFi disconnected"); lv_port_sem_give();
        }
        original_task_should_run_main_logic = false;
    }

    if (original_task_should_run_main_logic) {
        // --- MODIFICATION START: Use global tile coordinates ---
        ESP_LOGI(TAG, "Task: Download tile X:%d, Y:%d, Z:%d", g_current_tile_x, g_current_tile_y, g_current_tile_z);
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_port_sem_take();
            lv_label_set_text_fmt(status_label_simple, "Downloading X%d Y%d...", g_current_tile_x, g_current_tile_y);
            lv_port_sem_give();
        }
        // --- MODIFICATION END ---

        cleanup_tile_buffers();
        current_tile_data.is_http_transfer_active = true;
        current_tile_data.has_decoded_data = false;

        char url_buffer[256];
        // --- MODIFICATION START: Use global tile coordinates ---
        sprintf(url_buffer, TILE_URL_FORMAT, g_current_tile_x, g_current_tile_y, g_current_tile_z);
        // --- MODIFICATION END ---

        esp_http_client_config_t config = { // Kept as per user's original code
            .url = url_buffer,
            .event_handler = _http_event_handler_simple,
            .user_data = &current_tile_data,
            .timeout_ms = 20000,
            .buffer_size = 2048,
            .buffer_size_tx = 512,
            // NO .crt_bundle_attach here, to match user's provided code and avoid linker error
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client for X%d Y%d", g_current_tile_x, g_current_tile_y);
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                lv_port_sem_take(); lv_label_set_text(status_label_simple, "HTTP client error"); lv_port_sem_give();
            }
            // cleanup_tile_buffers(); // Not needed here as it's done at start or if client init fails
            original_task_should_run_main_logic = false; // Skip further processing in this task
        }


        if (original_task_should_run_main_logic) { // If client was initialized
            esp_err_t http_perform_err = esp_http_client_perform(client);
            bool actual_download_success = (http_perform_err == ESP_OK &&
                                          current_tile_data.downloaded_png_buf != NULL &&
                                          current_tile_data.downloaded_png_buf_size > 0);

            if (actual_download_success) {
                ESP_LOGI(TAG, "HTTP Perform OK for X%d Y%d. PNG data size: %u. Starting LodePNG decoding.",
                         g_current_tile_x, g_current_tile_y, (unsigned int)current_tile_data.downloaded_png_buf_size);
                if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                    lv_port_sem_take(); lv_label_set_text(status_label_simple, "Decoding..."); lv_port_sem_give();
                }

                unsigned char* temp_rgba_buf = NULL;
                unsigned int png_width = 0, png_height = 0;

                ESP_LOGD(TAG, "Heap before LodePNG: %d", (int)esp_get_free_heap_size());
                unsigned int lodepng_err = lodepng_decode32(&temp_rgba_buf, &png_width, &png_height,
                                                         current_tile_data.downloaded_png_buf,
                                                         current_tile_data.downloaded_png_buf_size);
                ESP_LOGD(TAG, "Heap after LodePNG (temp_rgba_buf at %p): %d", (void*)temp_rgba_buf, (int)esp_get_free_heap_size());

                if(current_tile_data.downloaded_png_buf) {
                    free(current_tile_data.downloaded_png_buf);
                    current_tile_data.downloaded_png_buf = NULL;
                    current_tile_data.downloaded_png_buf_size = 0;
                }

                if (lodepng_err) {
                    ESP_LOGE(TAG, "LodePNG error %u for X%d Y%d: %s", lodepng_err, g_current_tile_x, g_current_tile_y, lodepng_error_text(lodepng_err));
                    current_tile_data.has_decoded_data = false;
                    if(temp_rgba_buf) free(temp_rgba_buf);
                } else {
                    ESP_LOGI(TAG, "LodePNG decoded to RGBA for X%d Y%d: %ux%u", g_current_tile_x, g_current_tile_y, png_width, png_height);
                    if (png_width == TILE_SIZE && png_height == TILE_SIZE) {
                        current_tile_data.decoded_rgb565_buf = (uint8_t*)malloc(png_width * png_height * sizeof(uint16_t));
                        if (!current_tile_data.decoded_rgb565_buf) {
                            ESP_LOGE(TAG, "Failed to alloc for RGB565 data for X%d Y%d", g_current_tile_x, g_current_tile_y);
                            current_tile_data.has_decoded_data = false;
                        } else {
                            if (convert_rgba_to_rgb565(temp_rgba_buf, (uint16_t*)current_tile_data.decoded_rgb565_buf, png_width, png_height) == ESP_OK) {
                                current_tile_data.img_dsc.header.w = png_width;
                                current_tile_data.img_dsc.header.h = png_height;
                                current_tile_data.img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
                                current_tile_data.img_dsc.data = current_tile_data.decoded_rgb565_buf;
                                current_tile_data.img_dsc.data_size = png_width * png_height * sizeof(uint16_t);
                                current_tile_data.has_decoded_data = true;
                                ESP_LOGI(TAG, "Image ready as RGB565 for X%d Y%d.", g_current_tile_x, g_current_tile_y);
                            } else {
                                ESP_LOGE(TAG, "RGBA to RGB565 conversion failed for X%d Y%d.", g_current_tile_x, g_current_tile_y);
                                free(current_tile_data.decoded_rgb565_buf);
                                current_tile_data.decoded_rgb565_buf = NULL;
                                current_tile_data.has_decoded_data = false;
                            }
                        }
                        free(temp_rgba_buf);
                    } else {
                        ESP_LOGE(TAG, "Decoded PNG dim (%ux%u) != TILE_SIZE (%d) for X%d Y%d", png_width, png_height, TILE_SIZE, g_current_tile_x, g_current_tile_y);
                        free(temp_rgba_buf);
                        current_tile_data.has_decoded_data = false;
                    }
                }
            } else {
                ESP_LOGE(TAG, "Download failed or no data for X%d Y%d. HTTP Err: %s",
                         g_current_tile_x, g_current_tile_y, esp_err_to_name(http_perform_err));
                // cleanup_tile_buffers(); // No, done at start of next task. Partial buffers handled by HTTP events.
            }
            esp_http_client_cleanup(client);
        } // End if client was initialized
    } // End if task_should_run_main_logic

    // --- LVGL UI Update ---
    // Check original_task_should_run_main_logic to ensure we only attempt UI update if main processing part was intended
    if (original_task_should_run_main_logic && single_tile_img_obj && lv_obj_is_valid(single_tile_img_obj)) {
        lv_port_sem_take();
        if (current_tile_data.has_decoded_data && current_tile_data.decoded_rgb565_buf != NULL) {
            ESP_LOGI(TAG, "Displaying decoded RGB565 for X%d Y%d. Ptr: %p, Size: %u",
                     g_current_tile_x, g_current_tile_y, (void*)current_tile_data.img_dsc.data, (unsigned int)current_tile_data.img_dsc.data_size);

            lv_img_set_src(single_tile_img_obj, &current_tile_data.img_dsc);
            lv_obj_clear_flag(single_tile_img_obj, LV_OBJ_FLAG_HIDDEN);
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                // --- MODIFICATION START: Use global tile coordinates for status ---
                lv_label_set_text_fmt(status_label_simple, "Tile (%ux%u)\nX:%d Y:%d Z:%d",
                                      current_tile_data.img_dsc.header.w, current_tile_data.img_dsc.header.h,
                                      g_current_tile_x, g_current_tile_y, g_current_tile_z);
                // --- MODIFICATION END ---
            }
            ESP_LOGI(TAG, "Tile display command sent (RGB565 for X%d Y%d).", g_current_tile_x, g_current_tile_y);
        } else {
            ESP_LOGE(TAG, "No valid decoded data to display for X%d Y%d.", g_current_tile_x, g_current_tile_y);
            // lv_obj_add_flag(single_tile_img_obj, LV_OBJ_FLAG_HIDDEN); // Keep current visibility, just update status
            if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                 // --- MODIFICATION START: Use global tile coordinates for status ---
                lv_label_set_text_fmt(status_label_simple, "Failed X%d Y%d", g_current_tile_x, g_current_tile_y);
                 // --- MODIFICATION END ---
            }
        }
        lv_port_sem_give();
    } else if (!original_task_should_run_main_logic && status_label_simple && lv_obj_is_valid(status_label_simple)){
        // Status already updated for conditions like no WiFi or HTTP client init failure
        ESP_LOGI(TAG, "Task for X%d Y%d did not run main logic, UI not updated with new image.", g_current_tile_x, g_current_tile_y);
    } else if (!(single_tile_img_obj && lv_obj_is_valid(single_tile_img_obj))) {
         ESP_LOGE(TAG, "single_tile_img_obj is NULL or invalid, cannot update UI for X%d Y%d.", g_current_tile_x, g_current_tile_y);
    }

    // --- MODIFICATION START: Reset task running flag ---
    if (xSemaphoreTake(g_tile_op_mutex, portMAX_DELAY) == pdTRUE) {
        g_is_tile_task_running = false;
        ESP_LOGD(TAG, "Tile task (for target X%d Y%d) finished, flag cleared.", g_current_tile_x, g_current_tile_y);
        xSemaphoreGive(g_tile_op_mutex);
    } else {
        ESP_LOGE(TAG, "CRITICAL: Failed to take mutex to reset task running flag in task X%d Y%d!", g_current_tile_x, g_current_tile_y);
        g_is_tile_task_running = false; // Fallback direct clear, log indicates problem
    }
    // --- MODIFICATION END ---
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
    memset(&current_tile_data.img_dsc, 0, sizeof(lv_img_dsc_t));
}

// --- MODIFICATION START: Swipe event callback function ---
static void screen_swipe_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        bool coords_changed_by_swipe = false;
        int temp_prev_x = g_current_tile_x; // Store before trying to take mutex
        int temp_prev_y = g_current_tile_y;

        if (xSemaphoreTake(g_tile_op_mutex, pdMS_TO_TICKS(50)) == pdTRUE) { // 50ms timeout for mutex
            if (g_is_tile_task_running) {
                ESP_LOGW(TAG, "Swipe ignored: tile task is currently running for X%d Y%d.", g_current_tile_x, g_current_tile_y);
                if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                    lv_port_sem_take();
                    lv_label_set_text_fmt(status_label_simple, "Busy with X%d Y%d", g_current_tile_x, g_current_tile_y);
                    lv_port_sem_give();
                }
                xSemaphoreGive(g_tile_op_mutex);
                return;
            }

            // At this point, g_is_tile_task_running is false, and we hold the mutex.
            // temp_prev_x and temp_prev_y reflect the state *before* this swipe might change it.

            switch (dir) {
                case LV_DIR_LEFT:
                    ESP_LOGI(TAG, "Swipe Left Detected");
                    g_current_tile_x--;
                    coords_changed_by_swipe = true;
                    break;
                case LV_DIR_RIGHT:
                    ESP_LOGI(TAG, "Swipe Right Detected");
                    g_current_tile_x++;
                    coords_changed_by_swipe = true;
                    break;
                case LV_DIR_TOP: // Swipe upwards
                    ESP_LOGI(TAG, "Swipe Up Detected");
                    g_current_tile_y--; // Tile Y decreases upwards
                    coords_changed_by_swipe = true;
                    break;
                case LV_DIR_BOTTOM: // Swipe downwards
                    ESP_LOGI(TAG, "Swipe Down Detected");
                    g_current_tile_y++; // Tile Y increases downwards
                    coords_changed_by_swipe = true;
                    break;
                default:
                    break; // Not a swipe direction we act upon
            }

            if (coords_changed_by_swipe) {
                ESP_LOGI(TAG, "Tile coords target changed by swipe: X %d->%d, Y %d->%d",
                         temp_prev_x, g_current_tile_x, temp_prev_y, g_current_tile_y);

                if (!g_wifi_is_connected) {
                     ESP_LOGE(TAG, "WiFi not connected, cannot start new tile task from swipe.");
                     if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                        lv_port_sem_take(); lv_label_set_text(status_label_simple, "Swipe, but WiFi off"); lv_port_sem_give();
                     }
                     // Revert coordinates as we can't fetch the new tile
                     g_current_tile_x = temp_prev_x;
                     g_current_tile_y = temp_prev_y;
                } else {
                    g_is_tile_task_running = true; // Set flag: task is about to be created
                    BaseType_t task_created = xTaskCreate(download_and_decode_tile_task, "tile_dl_swipe", 1024 * 8, NULL, 5, NULL);
                    if (task_created != pdPASS) {
                        ESP_LOGE(TAG, "Failed to create new tile task from swipe!");
                        g_is_tile_task_running = false; // Reset flag as task creation failed
                        g_current_tile_x = temp_prev_x; // Revert coordinates
                        g_current_tile_y = temp_prev_y;
                        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                            lv_port_sem_take(); lv_label_set_text(status_label_simple, "Swipe task fail!"); lv_port_sem_give();
                        }
                    } else {
                        ESP_LOGI(TAG, "New tile task created by swipe for X%d Y%d", g_current_tile_x, g_current_tile_y);
                        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                           lv_port_sem_take();
                           lv_label_set_text_fmt(status_label_simple, "Swiped to X%d Y%d", g_current_tile_x, g_current_tile_y);
                           lv_port_sem_give();
                       }
                    }
                }
            }
            xSemaphoreGive(g_tile_op_mutex);
        } else {
             ESP_LOGW(TAG, "Could not obtain tile_op_mutex for swipe event in time.");
        }
    }
}
// --- MODIFICATION END ---


void my_ui_init_single_tile(void) {
    ESP_LOGI(TAG, "Initializing UI for LodePNG dynamic tile...");

    // --- MODIFICATION START: Initialize mutex and global tile coordinates ---
    if (g_tile_op_mutex == NULL) { // Create mutex only once
        g_tile_op_mutex = xSemaphoreCreateMutex();
        if (g_tile_op_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create tile operation mutex! Tile interaction will be unsafe.");
            return; // Cannot proceed safely
        }
    }

    g_current_tile_x = INITIAL_TILE_X; // Use macro from .h for initial value
    g_current_tile_y = INITIAL_TILE_Y;
    g_current_tile_z = INITIAL_TILE_Z;
    g_is_tile_task_running = false; // Ensure flag is reset on init
    // --- MODIFICATION END ---

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

    // --- MODIFICATION START: Add gesture event listener and manage initial task creation with mutex ---
    lv_obj_add_event_cb(scr, screen_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    // lv_obj_clear_flag(scr, LV_OBJ_FLAG_GESTURE_BUBBLE); // Default is usually fine, enables gesture processing on parent if child doesn't handle
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE); // Important: ensures screen processes input events needed for gestures

    if (g_wifi_is_connected) {
        ESP_LOGI(TAG, "WiFi connected, creating initial download & decode task for X%d Y%d Z%d.",
                 g_current_tile_x, g_current_tile_y, g_current_tile_z);
        if (xSemaphoreTake(g_tile_op_mutex, portMAX_DELAY) == pdTRUE) {
            if (g_is_tile_task_running) {
                ESP_LOGW(TAG, "Initial task creation: A tile task is already marked as running. This is unexpected on init.");
                // Consider what to do here. Forcing flag to false if this is a true init might be an option.
                // For now, just log. The task will proceed if flag is false.
            }
            // Ensure flag is false before starting the first task, in case of re-init.
            // g_is_tile_task_running = false; // Already done above during global var init

            g_is_tile_task_running = true; // Mark as running BEFORE creating task
            BaseType_t task_created = xTaskCreate(download_and_decode_tile_task, "tile_dl_decode", 1024 * 8, NULL, 5, NULL);
            if (task_created != pdPASS) {
                ESP_LOGE(TAG, "Failed to create initial download_and_decode_tile_task!");
                g_is_tile_task_running = false; // Reset flag if task creation failed
                if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                    lv_label_set_text(status_label_simple, "Task creation failed!");
                }
            }
            xSemaphoreGive(g_tile_op_mutex);
        } else {
            ESP_LOGE(TAG, "Failed to take mutex for initial task creation.");
             if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
                lv_label_set_text(status_label_simple, "Mutex error on init!");
            }
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected at UI init.");
        if (status_label_simple && lv_obj_is_valid(status_label_simple)) {
            lv_label_set_text(status_label_simple, "Connect to WiFi...");
        }
    }
    // --- MODIFICATION END ---
}