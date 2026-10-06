/**
 * @file bme280_driver.h
 * @brief Driver I2C cho cảm biến BME280 (nhiệt độ, độ ẩm, áp suất)
 *
 * Hỗ trợ cả BME280 và SHT31 (chuyển bằng Kconfig hoặc define).
 * Giao tiếp qua I2C, đọc dữ liệu compensated.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dữ liệu đọc từ cảm biến
 */
typedef struct {
    float temperature;  /**< Nhiệt độ (°C) */
    float humidity;     /**< Độ ẩm tương đối (%) */
    float pressure;     /**< Áp suất khí quyển (hPa), chỉ BME280 */
} bme280_data_t;

/**
 * @brief Cấu hình khởi tạo driver
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;  /**< Handle I2C bus đã khởi tạo */
    uint8_t dev_addr;                 /**< Địa chỉ I2C thiết bị (mặc định 0x76 hoặc 0x77) */
} bme280_config_t;

/** Địa chỉ I2C mặc định của BME280 */
#define BME280_I2C_ADDR_PRIMARY   0x76
#define BME280_I2C_ADDR_SECONDARY 0x77

/**
 * @brief Khởi tạo driver BME280
 *
 * @param config Cấu hình I2C bus và địa chỉ
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t bme280_init(const bme280_config_t *config);

/**
 * @brief Đọc dữ liệu cảm biến (forced mode)
 *
 * @param[out] data Con trỏ nhận dữ liệu nhiệt độ/ẩm/áp suất
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t bme280_read(bme280_data_t *data);

/**
 * @brief Đặt cảm biến vào sleep mode để tiết kiệm năng lượng
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t bme280_sleep(void);

/**
 * @brief Giải phóng tài nguyên driver
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t bme280_deinit(void);

#ifdef __cplusplus
}
#endif

