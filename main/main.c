// app_main.c

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "bsp_board.h"    // Ensure your board support package header is included
#include "lv_demos.h"     // May be needed by lv_port, or can be removed if lv_port is modified
#include "lv_port.h"
#include "my_ui.h"        // Your custom UI header

#include "driver/uart.h"  // For UART communication
#include <string.h>       // For string operations
#include <stdlib.h>       // For malloc, free

#define LOG_MEM_INFO        1 // Enable memory logging

static const char *TAG_MAIN = "app_main_dynamic_ui";

// --- UART Configuration ---
#define UART_PORT_NUM      UART_NUM_0 // Use UART0 (typically connected to USB-to-Serial)
#define UART_RX_BUF_SIZE   (2048)     // UART driver's internal RX ring buffer size
#define UART_TX_BUF_SIZE   (256)      // UART driver's internal TX ring buffer size (if sending responses)
#define JSON_RX_BUFFER_SIZE (1536)    // Buffer to store the received JSON string (should be less than UART_RX_BUF_SIZE)

// UART event queue handle
static QueueHandle_t uart_event_queue;
// Static buffer for assembling JSON data
static char json_data_buffer[JSON_RX_BUFFER_SIZE + 1]; // +1 for null terminator
static int json_data_buffer_index = 0;

/**
 * @brief UART event handling task
 *
 * Listens for UART events, receives data, and when a newline is detected,
 * attempts to parse it as JSON and update the UI.
 */
static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t* temp_uart_buffer = (uint8_t*) malloc(UART_RX_BUF_SIZE); // Temporary buffer for uart_read_bytes
    if (!temp_uart_buffer) {
        ESP_LOGE(TAG_MAIN, "Failed to allocate UART temporary buffer for reading!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG_MAIN, "UART event task started. Waiting for JSON data ending with newline on UART%d...", UART_PORT_NUM);
    ESP_LOGI(TAG_MAIN, "Baud rate: 115200. Send JSON string followed by a newline character.");

    for (;;) {
        // Wait for UART event
        if (xQueueReceive(uart_event_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(UART_PORT_NUM, temp_uart_buffer, event.size, portMAX_DELAY);
                    ESP_LOGD(TAG_MAIN, "UART_DATA received %d bytes", event.size);

                    for (int i = 0; i < event.size; i++) {
                        char received_char = temp_uart_buffer[i];

                        if (received_char == '\n') { // Newline signifies end of JSON string
                            if (json_data_buffer_index > 0 && json_data_buffer_index <= JSON_RX_BUFFER_SIZE) {
                                json_data_buffer[json_data_buffer_index] = '\0'; // Null-terminate
                                ESP_LOGI(TAG_MAIN, "Complete JSON string received (%d bytes). Processing...", json_data_buffer_index);
                                ESP_LOGD(TAG_MAIN, "JSON: %s", json_data_buffer);

                                lv_port_sem_take(); // Take LVGL semaphore
                                bool success = my_ui_render_from_json(json_data_buffer);
                                lv_port_sem_give(); // Release LVGL semaphore

                                if (success) {
                                    ESP_LOGI(TAG_MAIN, "UI rendered successfully from serial JSON.");
                                    // Optionally send ACK back via UART
                                    // uart_write_bytes(UART_PORT_NUM, "OK: UI Updated\n", 15);
                                } else {
                                    ESP_LOGE(TAG_MAIN, "Failed to render UI from serial JSON.");
                                    // uart_write_bytes(UART_PORT_NUM, "ERROR: UI Update Failed\n", 24);
                                }
                            } else if (json_data_buffer_index > JSON_RX_BUFFER_SIZE) {
                                ESP_LOGE(TAG_MAIN, "JSON data buffer was overflowed. Discarded.");
                            } else {
                                // ESP_LOGI(TAG_MAIN, "Received empty line or only newline."); // Can be noisy
                            }
                            json_data_buffer_index = 0; // Reset buffer index for the next JSON
                        } else if (received_char == '\r') {
                            // Ignore carriage return
                        } else {
                            // Store character in buffer
                            if (json_data_buffer_index < JSON_RX_BUFFER_SIZE) {
                                json_data_buffer[json_data_buffer_index++] = received_char;
                            } else {
                                // Buffer is full, but not yet marked as overflowed before this char
                                if (json_data_buffer_index == JSON_RX_BUFFER_SIZE) { // First char that causes overflow
                                    ESP_LOGE(TAG_MAIN, "JSON data buffer overflow! Max %d chars. Ignoring further chars until newline.", JSON_RX_BUFFER_SIZE);
                                    json_data_buffer_index++; // Mark as overflowed (idx > JSON_RX_BUFFER_SIZE)
                                }
                                // While overflowed, further characters are ignored until a newline resets the buffer
                            }
                        }
                    }
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG_MAIN, "UART HW FIFO overflow. Flushing input.");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_event_queue); // Reset queue
                    json_data_buffer_index = 0;    // Reset buffer
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG_MAIN, "UART ring buffer full. Flushing input.");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_event_queue); // Reset queue
                    json_data_buffer_index = 0;    // Reset buffer
                    break;

                case UART_BREAK:
                    ESP_LOGW(TAG_MAIN, "UART RX break");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG_MAIN, "UART parity error");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG_MAIN, "UART frame error");
                    break;
                default:
                    ESP_LOGI(TAG_MAIN, "Unhandled UART event type: %d", event.type);
                    break;
            }
        }
    }
    free(temp_uart_buffer);
    temp_uart_buffer = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Initialize UART for receiving JSON data
 */
void app_uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver, and get the queue.
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 20, &uart_event_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    // Set UART pins. UART0 usually uses default pins shared with bootloader/logging.
    // If using a different UART or custom pins, configure them here.
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG_MAIN, "UART%d initialized (baud 115200). Ready for JSON input.", UART_PORT_NUM);
}


// Example JSON string for initial display (in English)
const char *initial_sample_json_ui = "{\
  \"screen_id\": \"initial_screen\",\
  \"background_color\": \"#E0E0E0\",\
  \"layout\": {\"type\": \"column\", \"padding\": 20, \"gap\": 15, \"item_alignment\": \"center_horizontal\", \"distribution\": \"center\"},\
  \"elements\": [\
    {\
      \"type\": \"label\",\
      \"id\": \"lbl_status\",\
      \"text\": \"Waiting for serial JSON data...\",\
      \"style_ref\": \"theme.font.large_title\",\
      \"text_color\": \"#333333\"\
    }\
  ]\
}";


void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "System start");

    ESP_ERROR_CHECK(bsp_board_init()); // Initialize board-specifics
    lv_port_init();                    // Initialize LVGL port
    my_ui_styles_init();               // Initialize custom UI styles (uses generic English fonts)

    app_uart_init();                   // Initialize UART
    // Create UART event handling task
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL); // Increased stack for safety


#if CONFIG_LCD_AVOID_TEAR // LCD tearing effect configuration
    ESP_LOGI(TAG_MAIN, "Avoid lcd tearing effect");
#if CONFIG_LCD_LVGL_FULL_REFRESH
    ESP_LOGI(TAG_MAIN, "LVGL full-refresh");
#elif CONFIG_LCD_LVGL_DIRECT_MODE
    ESP_LOGI(TAG_MAIN, "LVGL direct-mode");
#endif
#endif

    lv_port_sem_take(); // Take LVGL semaphore

    // --- Render an initial UI from hardcoded JSON (optional) ---
    bool success = my_ui_render_from_json(initial_sample_json_ui);
    if (success) {
        ESP_LOGI(TAG_MAIN, "Successfully rendered initial UI from hardcoded JSON.");
    } else {
        ESP_LOGE(TAG_MAIN, "Failed to render initial UI from hardcoded JSON.");
    }

    lv_port_sem_give(); // Release LVGL semaphore

#if LOG_MEM_INFO
    static char mem_log_buffer[128];
    while (1) {
        sprintf(mem_log_buffer, "   Biggest /     Free /    Total\n"
                "\t  DRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM_INFO", "%s", mem_log_buffer); // Use a different TAG for memory info

        vTaskDelay(pdMS_TO_TICKS(10000)); // Print memory info every 10 seconds
    }
#endif
}