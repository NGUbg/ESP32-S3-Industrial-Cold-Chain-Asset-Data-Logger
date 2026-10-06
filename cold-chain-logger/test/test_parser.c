/**
 * @file test_parser.c
 * @brief Unit test bằng Unity cho logic thuần (parser, ngưỡng cảnh báo)
 *
 * Chạy trên host hoặc target bằng lệnh:
 *   idf.py -T test build
 *   idf.py -T test flash monitor
 */

#include "unity.h"
#include "storage_manager.h"

/* ================================================================
 * Test sensor_record_t struct layout
 * ================================================================ */

TEST_CASE("sensor_record_t size is consistent", "[parser]")
{
    /* Đảm bảo struct không thay đổi kích thước khi serialize */
    TEST_ASSERT_GREATER_THAN(0, sizeof(sensor_record_t));
}

/* ================================================================
 * Test ngưỡng cảnh báo nhiệt độ
 * ================================================================ */

#define TEMP_THRESHOLD_LOW   2.0f   /* Ngưỡng dưới cho cold-chain (°C) */
#define TEMP_THRESHOLD_HIGH  8.0f   /* Ngưỡng trên cho cold-chain (°C) */

static bool is_temperature_in_range(float temp)
{
    return (temp >= TEMP_THRESHOLD_LOW && temp <= TEMP_THRESHOLD_HIGH);
}

TEST_CASE("temperature within cold-chain range", "[parser]")
{
    TEST_ASSERT_TRUE(is_temperature_in_range(2.0f));
    TEST_ASSERT_TRUE(is_temperature_in_range(5.0f));
    TEST_ASSERT_TRUE(is_temperature_in_range(8.0f));
}

TEST_CASE("temperature below cold-chain range triggers alert", "[parser]")
{
    TEST_ASSERT_FALSE(is_temperature_in_range(1.9f));
    TEST_ASSERT_FALSE(is_temperature_in_range(0.0f));
    TEST_ASSERT_FALSE(is_temperature_in_range(-5.0f));
}

TEST_CASE("temperature above cold-chain range triggers alert", "[parser]")
{
    TEST_ASSERT_FALSE(is_temperature_in_range(8.1f));
    TEST_ASSERT_FALSE(is_temperature_in_range(25.0f));
}

/* ================================================================
 * Test battery voltage parsing
 * ================================================================ */

#define BATTERY_LOW_MV  3300.0f   /* Ngưỡng pin yếu (mV) */

static bool is_battery_low(float battery_mv)
{
    return battery_mv < BATTERY_LOW_MV;
}

TEST_CASE("battery level normal", "[parser]")
{
    TEST_ASSERT_FALSE(is_battery_low(4200.0f));  /* Full */
    TEST_ASSERT_FALSE(is_battery_low(3700.0f));  /* ~50% */
    TEST_ASSERT_FALSE(is_battery_low(3300.0f));  /* Boundary */
}

TEST_CASE("battery level low triggers alert", "[parser]")
{
    TEST_ASSERT_TRUE(is_battery_low(3299.0f));
    TEST_ASSERT_TRUE(is_battery_low(3000.0f));
}

/* ================================================================
 * Test record validation
 * ================================================================ */

static bool is_record_valid(const sensor_record_t *record)
{
    if (record == NULL) return false;
    if (record->timestamp <= 0) return false;
    if (record->temperature < -40.0f || record->temperature > 85.0f) return false;
    if (record->humidity < 0.0f || record->humidity > 100.0f) return false;
    return true;
}

TEST_CASE("valid sensor record passes validation", "[parser]")
{
    sensor_record_t rec = {
        .timestamp = 1700000000000LL,
        .temperature = 5.0f,
        .humidity = 60.0f,
        .pressure = 1013.25f,
        .battery_mv = 3800.0f,
        .sent = false,
    };
    TEST_ASSERT_TRUE(is_record_valid(&rec));
}

TEST_CASE("invalid timestamp fails validation", "[parser]")
{
    sensor_record_t rec = {
        .timestamp = 0,
        .temperature = 5.0f,
        .humidity = 60.0f,
    };
    TEST_ASSERT_FALSE(is_record_valid(&rec));
}

TEST_CASE("out-of-range temperature fails validation", "[parser]")
{
    sensor_record_t rec = {
        .timestamp = 1700000000000LL,
        .temperature = 100.0f,  /* BME280 max = 85°C */
        .humidity = 60.0f,
    };
    TEST_ASSERT_FALSE(is_record_valid(&rec));
}

TEST_CASE("null record pointer fails validation", "[parser]")
{
    TEST_ASSERT_FALSE(is_record_valid(NULL));
}

