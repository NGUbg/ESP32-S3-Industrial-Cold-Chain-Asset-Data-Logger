#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "cold-chain logger boot, free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());

    uint32_t count = 0;
    while (1) {
        ESP_LOGI(TAG, "alive: count=%lu, tick=%lu",
                 (unsigned long)count,
                 (unsigned long)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1000));
        count++;

    }
    
}