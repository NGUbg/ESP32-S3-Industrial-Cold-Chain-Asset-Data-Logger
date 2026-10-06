/**
 * @file storage_manager.c
 * @brief Triển khai wrapper LittleFS + logic store-and-forward
 *
 * Lưu bản ghi cảm biến dưới dạng binary file trên LittleFS.
 * Mỗi file chứa 1 batch bản ghi, tên file = timestamp đầu tiên.
 * Khi gửi thành công, file được xóa.
 */

#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "storage_mgr";

#define STORAGE_BASE_PATH   "/littlefs"
#define STORAGE_PARTITION   "storage"
#define RECORDS_DIR         STORAGE_BASE_PATH "/records"
#define MAX_FILENAME_LEN    64

static bool s_initialized = false;

esp_err_t storage_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Mount LittleFS */
    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = STORAGE_PARTITION,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Kiểm tra/tạo thư mục records */
    struct stat st;
    if (stat(RECORDS_DIR, &st) != 0) {
        if (mkdir(RECORDS_DIR, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create records directory");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Created records directory");
    }

    /* Log thông tin dung lượng */
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(STORAGE_PARTITION, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: total=%zu, used=%zu, free=%zu bytes",
                 total, used, total - used);
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t storage_manager_store(const sensor_record_t *record)
{
    if (!s_initialized || record == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Tạo filename từ timestamp */
    char filepath[MAX_FILENAME_LEN];
    snprintf(filepath, sizeof(filepath), RECORDS_DIR "/%" PRId64 ".bin", record->timestamp);

    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        return ESP_FAIL;
    }

    /* Ghi bản ghi (đánh dấu chưa gửi) */
    sensor_record_t rec_copy = *record;
    rec_copy.sent = false;

    size_t written = fwrite(&rec_copy, sizeof(sensor_record_t), 1, f);
    fclose(f);

    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write record to file");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Stored record at %" PRId64, record->timestamp);
    return ESP_OK;
}

esp_err_t storage_manager_get_pending(sensor_record_t *records, size_t max_count, size_t *actual_count)
{
    if (!s_initialized || records == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *actual_count = 0;
    DIR *dir = opendir(RECORDS_DIR);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open records directory");
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *actual_count < max_count) {
        /* Chỉ đọc file .bin */
        if (strstr(entry->d_name, ".bin") == NULL) {
            continue;
        }

        char filepath[MAX_FILENAME_LEN];
        snprintf(filepath, sizeof(filepath), RECORDS_DIR "/%s", entry->d_name);

        FILE *f = fopen(filepath, "rb");
        if (f == NULL) {
            continue;
        }

        sensor_record_t rec;
        if (fread(&rec, sizeof(sensor_record_t), 1, f) == 1 && !rec.sent) {
            records[*actual_count] = rec;
            (*actual_count)++;
        }
        fclose(f);
    }

    closedir(dir);
    ESP_LOGD(TAG, "Found %zu pending records", *actual_count);
    return ESP_OK;
}

esp_err_t storage_manager_mark_sent(const int64_t *timestamps, size_t count)
{
    if (!s_initialized || timestamps == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t deleted = 0;
    for (size_t i = 0; i < count; i++) {
        char filepath[MAX_FILENAME_LEN];
        snprintf(filepath, sizeof(filepath), RECORDS_DIR "/%" PRId64 ".bin", timestamps[i]);

        /* Xóa file luôn thay vì đánh dấu — tiết kiệm dung lượng */
        if (remove(filepath) == 0) {
            deleted++;
        } else {
            ESP_LOGW(TAG, "Failed to remove record file: %s", filepath);
        }
    }

    ESP_LOGI(TAG, "Marked %zu/%zu records as sent (deleted)", deleted, count);
    return ESP_OK;
}

esp_err_t storage_manager_get_stats(size_t *total_bytes, size_t *used_bytes)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_littlefs_info(STORAGE_PARTITION, total_bytes, used_bytes);
}

esp_err_t storage_manager_erase_all(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Xóa tất cả file trong records directory */
    DIR *dir = opendir(RECORDS_DIR);
    if (dir == NULL) {
        return ESP_FAIL;
    }

    struct dirent *entry;
    size_t count = 0;
    while ((entry = readdir(dir)) != NULL) {
        char filepath[MAX_FILENAME_LEN];
        snprintf(filepath, sizeof(filepath), RECORDS_DIR "/%s", entry->d_name);
        if (remove(filepath) == 0) {
            count++;
        }
    }
    closedir(dir);

    ESP_LOGI(TAG, "Erased %zu record files", count);
    return ESP_OK;
}

esp_err_t storage_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_littlefs_unregister(STORAGE_PARTITION);
    s_initialized = false;
    ESP_LOGI(TAG, "Storage manager deinitialized");
    return ret;
}

