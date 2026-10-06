/**
 * @file bme280_driver.c
 * @brief Triển khai driver I2C cho cảm biến BME280
 *
 * TODO: Implement register read/write, compensation formulas,
 *       và forced mode measurement cycle.
 */

#include "bme280_driver.h"
#include "esp_log.h"

static const char *TAG = "bme280";

/* --- BME280 Register Map --- */
#define BME280_REG_CHIP_ID     0xD0
#define BME280_REG_CTRL_HUM    0xF2
#define BME280_REG_CTRL_MEAS   0xF4
#define BME280_REG_CONFIG      0xF5
#define BME280_REG_PRESS_MSB   0xF7
#define BME280_REG_TEMP_MSB    0xFA
#define BME280_REG_HUM_MSB     0xFD

#define BME280_CHIP_ID_VALUE   0x60

/* --- Internal state --- */
static i2c_master_dev_handle_t s_dev_handle = NULL;

/* --- Calibration data (đọc từ NVM registers khi init) --- */
/* TODO: Khai báo struct cho compensation parameters */

esp_err_t bme280_init(const bme280_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Thêm device vào I2C bus */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->dev_addr,
        .scl_speed_hz = 100000,  /* BME280 hỗ trợ tới 3.4MHz, dùng 100kHz cho ổn định */
    };

    esp_err_t ret = i2c_master_bus_add_device(config->i2c_bus, &dev_cfg, &s_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Đọc chip ID để xác nhận giao tiếp */
    uint8_t reg = BME280_REG_CHIP_ID;
    uint8_t chip_id = 0;
    ret = i2c_master_transmit_receive(s_dev_handle, &reg, 1, &chip_id, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
        return ret;
    }

    if (chip_id != BME280_CHIP_ID_VALUE) {
        ESP_LOGE(TAG, "Unexpected chip ID: 0x%02X (expected 0x%02X)", chip_id, BME280_CHIP_ID_VALUE);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "BME280 detected, chip ID: 0x%02X", chip_id);

    /* TODO: Đọc calibration data từ registers 0x88-0xA1, 0xE1-0xE7 */
    /* TODO: Cấu hình oversampling, filter, standby time */

    return ESP_OK;
}

esp_err_t bme280_read(bme280_data_t *data)
{
    if (data == NULL || s_dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* TODO: Trigger forced measurement (ghi CTRL_MEAS) */
    /* TODO: Đợi measurement complete */
    /* TODO: Đọc raw data từ registers 0xF7-0xFE (8 bytes) */
    /* TODO: Áp dụng compensation formulas từ datasheet */

    /* Placeholder — trả về 0 cho đến khi implement */
    data->temperature = 0.0f;
    data->humidity = 0.0f;
    data->pressure = 0.0f;

    ESP_LOGW(TAG, "bme280_read() not fully implemented yet");
    return ESP_OK;
}

esp_err_t bme280_sleep(void)
{
    if (s_dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* TODO: Ghi sleep mode vào CTRL_MEAS register (mode bits = 00) */
    ESP_LOGW(TAG, "bme280_sleep() not fully implemented yet");
    return ESP_OK;
}

esp_err_t bme280_deinit(void)
{
    if (s_dev_handle != NULL) {
        i2c_master_bus_rm_device(s_dev_handle);
        s_dev_handle = NULL;
    }
    ESP_LOGI(TAG, "BME280 driver deinitialized");
    return ESP_OK;
}

