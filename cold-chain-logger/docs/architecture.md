# Kiến trúc hệ thống — ESP32-S3 Cold-Chain Logger

## Tổng quan

Firmware dựa trên **ESP-IDF (FreeRTOS)** với 4 task chính giao tiếp qua FreeRTOS Queue.

## Sơ đồ luồng dữ liệu

```
                    ┌────────────────────────────────────────┐
                    │              ESP32-S3 (FreeRTOS)         │
                    │                                          │
  Cảm biến I2C  ──▶ │  [sensor_task] ──Queue──▶ [storage_task] │
  (BME280/SHT31)    │        │                       │         │
                    │        │                  LittleFS (lưu  │
                    │        │                  khi mất mạng)  │
                    │        ▼                       │         │
                    │  [health_task]            Queue│         │
                    │  (watchdog, pin,               ▼         │
                    │   brown-out)          [network_task]     │
                    │                        │        │        │
                    └────────────────────────┼────────┼────────┘
                                              ▼        ▼
                                      MQTT over TLS   OTA HTTPS
                                              │
                                              ▼
                                     Broker / Backend / Dashboard
```

## Task chi tiết

| Task | Priority | Stack | Vai trò |
|---|---|---|---|
| `sensor_task` | 5 | 4096 | Đọc BME280 qua I2C, đẩy `sensor_record_t` vào Queue |
| `storage_task` | 4 | 4096 | Nhận từ Queue, ghi LittleFS, quản lý store-and-forward |
| `network_task` | 3 | 8192 | Wi-Fi/MQTT/TLS, gửi dữ liệu tồn đọng, nhận lệnh OTA |
| `health_task` | 6 | 2048 | Feed watchdog, đọc ADC pin, log trạng thái |

## Cơ chế Store-and-Forward

1. `sensor_task` đọc cảm biến → gửi bản ghi vào `xSensorQueue`
2. `storage_task` nhận bản ghi → ghi vào LittleFS dưới dạng file `.bin`
3. Khi `network_task` báo có mạng → `storage_task` đọc bản ghi pending → gửi vào `xNetworkQueue`
4. `network_task` gửi qua MQTT/TLS → báo kết quả → `storage_task` xóa file đã gửi thành công

## Partition Layout

| Name | Type | Size | Mô tả |
|---|---|---|---|
| nvs | data | 24KB | Non-volatile storage (cấu hình, credentials) |
| otadata | data | 8KB | OTA partition tracking |
| ota_0 | app | 1.75MB | Firmware slot A |
| ota_1 | app | 1.75MB | Firmware slot B (rollback) |
| storage | data | 384KB | LittleFS store-and-forward |

