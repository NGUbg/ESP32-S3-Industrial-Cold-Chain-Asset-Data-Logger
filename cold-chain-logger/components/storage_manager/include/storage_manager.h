/**
 * @file storage_manager.h
 * @brief Wrapper LittleFS + logic store-and-forward
 *
 * Quản lý lưu trữ bản ghi cảm biến vào LittleFS khi mất mạng,
 * và truy xuất các bản ghi chưa gửi khi có mạng trở lại.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Một bản ghi dữ liệu cảm biến
 */
typedef struct {
    int64_t timestamp;    /**< Unix timestamp (ms) */
    float temperature;    /**< Nhiệt độ (°C) */
    float humidity;       /**< Độ ẩm (%) */
    float pressure;       /**< Áp suất (hPa) */
    float battery_mv;     /**< Điện áp pin (mV) */
    bool sent;            /**< Đã gửi lên server chưa */
} sensor_record_t;

/**
 * @brief Khởi tạo LittleFS partition và cấu trúc thư mục
 *
 * Mount LittleFS partition "storage", tạo thư mục nếu chưa có.
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_init(void);

/**
 * @brief Lưu một bản ghi vào LittleFS
 *
 * Ghi bản ghi vào file, đánh dấu sent = false.
 * Tự quản lý rotation khi hết dung lượng (xóa bản ghi cũ nhất đã gửi).
 *
 * @param record Bản ghi cần lưu
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_store(const sensor_record_t *record);

/**
 * @brief Lấy danh sách bản ghi chưa gửi (pending)
 *
 * @param[out] records Mảng buffer nhận bản ghi
 * @param max_count Số lượng tối đa cần lấy
 * @param[out] actual_count Số lượng thực tế trả về
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_get_pending(sensor_record_t *records, size_t max_count, size_t *actual_count);

/**
 * @brief Đánh dấu các bản ghi đã gửi thành công
 *
 * @param timestamps Mảng timestamp của các bản ghi cần đánh dấu
 * @param count Số lượng bản ghi
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_mark_sent(const int64_t *timestamps, size_t count);

/**
 * @brief Lấy thông tin dung lượng lưu trữ
 *
 * @param[out] total_bytes Tổng dung lượng partition
 * @param[out] used_bytes Dung lượng đã sử dụng
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_get_stats(size_t *total_bytes, size_t *used_bytes);

/**
 * @brief Xóa toàn bộ dữ liệu lưu trữ (dùng khi factory reset)
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_erase_all(void);

/**
 * @brief Giải phóng tài nguyên, unmount LittleFS
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t storage_manager_deinit(void);

#ifdef __cplusplus
}
#endif

