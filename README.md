# ESP32-S3 Industrial Cold-Chain & Asset Data Logger

> Firmware-first IoT data logger cho bài toán giám sát chuỗi lạnh (cold-chain) / tài sản di động — thiết kế theo đúng các ràng buộc kỹ thuật mà một hệ thống nhúng chạy thật ngoài sản xuất cần có: đa nhiệm thời gian thực (RTOS), hoạt động ổn định khi mất mạng, bảo mật truyền thông, cập nhật OTA an toàn, và tối ưu năng lượng.

Dự án này được xây dựng như bước nâng cấp có chủ đích sau capstone **Smart Home & AI Camera cho người cao tuổi** (Samsung Innovation Campus), nhằm lấp các khoảng trống kỹ thuật của firmware cũ (single-loop Arduino, publish một chiều, không bảo mật, không OTA).

---

## 1. Bài toán

Trong vận chuyển/lưu kho hàng nhạy cảm với nhiệt độ (dược phẩm, thực phẩm đông lạnh, thiết bị y tế), cần một thiết bị:
- Ghi lại nhiệt độ/độ ẩm liên tục, kể cả khi **không có kết nối mạng** trong thời gian dài (xe tải di chuyển, kho không có Wi-Fi).
- Tự động **đồng bộ dữ liệu** khi có kết nối trở lại, không được mất dữ liệu đã ghi.
- Cảnh báo ngay khi vượt ngưỡng an toàn.
- Chạy được **nhiều tháng bằng pin**.
- Có thể **cập nhật firmware từ xa** khi triển khai số lượng lớn (không thể thu hồi thiết bị để nạp lại bằng tay).

## 2. Vì sao chọn bài toán này để luyện kỹ năng

| Yêu cầu bài toán | Kỹ năng firmware tương ứng |
|---|---|
| Ghi liên tục kể cả mất mạng | Store-and-forward, file system nhúng (LittleFS) |
| Không mất dữ liệu khi crash/mất điện | NVS, watchdog, brown-out detection |
| Đồng thời đọc cảm biến + quản lý lưu trữ + mạng | FreeRTOS đa task, Queue, Mutex |
| Chạy nhiều tháng bằng pin | Deep sleep, đo & tối ưu dòng tiêu thụ |
| Cập nhật số lượng lớn thiết bị từ xa | OTA qua HTTPS, cơ chế rollback |
| Dữ liệu nhạy cảm khi truyền qua mạng công cộng | MQTT qua TLS, không anonymous |
| Độ tin cậy khi vận hành vô nhân sự | Unit test, CI, error handling có hệ thống |

---

## 3. Kiến trúc hệ thống

```
                    ┌────────────────────────────────────────┐
                    │              ESP32-S3 (FreeRTOS)         │
                    │                                          │
  Cảm biến I2C  ──▶ │  [sensor_task] ──Queue──▶ [storage_task] │
  (nhiệt độ/ẩm)     │        │                       │         │
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

4 task chính, giao tiếp qua FreeRTOS Queue (không dùng biến toàn cục chia sẻ trực tiếp):

| Task | Vai trò |
|---|---|
| `sensor_task` | Đọc cảm biến I2C định kỳ, đẩy bản ghi vào Queue |
| `storage_task` | Nhận bản ghi, ghi vào LittleFS nếu chưa có mạng, đánh dấu đã gửi khi thành công |
| `network_task` | Kết nối Wi-Fi/MQTT qua TLS, gửi dữ liệu tồn đọng khi có mạng, nhận lệnh OTA |
| `health_task` | Feed watchdog, theo dõi điện áp pin, log trạng thái hệ thống định kỳ |

---

## 4. Yêu cầu kỹ thuật bắt buộc (Definition of Done)

- [ ] 100% viết bằng **ESP-IDF thuần** (không Arduino framework).
- [ ] Tối thiểu **4 FreeRTOS task** như kiến trúc ở trên, giao tiếp qua Queue/Mutex.
- [ ] **Store-and-forward**: dữ liệu không bị mất khi mất mạng, tự đồng bộ khi có mạng lại.
- [ ] **MQTT qua TLS** với chứng chỉ thật, không dùng kết nối anonymous.
- [ ] **OTA update qua HTTPS** với cơ chế rollback nếu firmware mới lỗi/không xác nhận được.
- [ ] **Deep sleep** giữa các chu kỳ đo, có số liệu đo dòng tiêu thụ thực tế trước/sau tối ưu.
- [ ] **Watchdog timer** + xử lý brown-out, tự phục hồi sau khi crash (test bằng cách chủ động gây lỗi/reset).
- [ ] **Unit test** (Unity) cho toàn bộ logic thuần (parser, ngưỡng cảnh báo...), chạy được trong CI không cần phần cứng thật.
- [ ] **BLE Provisioning** để cấu hình Wi-Fi, không hard-code SSID/password trong source code.
- [ ] Tài liệu kiến trúc: sơ đồ task/luồng dữ liệu, state diagram cho storage/network, bảng số liệu tiêu thụ năng lượng.

---

## 5. Phần cứng

| Thành phần | Vai trò |
|---|---|
| ESP32-S3-DevKitC-1 (PSRAM) | MCU chính, chạy FreeRTOS |
| Cảm biến nhiệt độ/độ ẩm I2C (BME280 hoặc SHT31) | Nguồn dữ liệu chính |
| Pin Li-ion 18650 + module sạc/bảo vệ (TP4056) | Nguồn nuôi độc lập, đo thời lượng hoạt động |
| Module đo điện áp pin (ADC chia áp) | Theo dõi mức pin cho `health_task` |
| (Tùy chọn) Module RTC ngoài (DS3231) | Giữ thời gian chính xác khi deep sleep dài |

---

## 6. Cấu trúc thư mục dự kiến

```
cold-chain-logger/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── app_main.c              # khởi tạo task, queue
│   ├── sensor_task.c/.h
│   ├── storage_task.c/.h
│   ├── network_task.c/.h
│   └── health_task.c/.h
├── components/
│   ├── bme280_driver/          # driver I2C tự viết
│   └── storage_manager/        # wrapper LittleFS + logic store-and-forward
├── test/
│   └── test_parser.c           # unit test bằng Unity
├── docs/
│   ├── architecture.md
│   ├── power-measurements.md
│   └── state-diagram.png
└── .github/workflows/build.yml # CI build tự động
```

---

## 7. Build & Flash

```bash
# Cài ESP-IDF (1 lần)
. $HOME/esp/esp-idf/export.sh

# Cấu hình target
idf.py set-target esp32s3

# Build
idf.py build

# Flash + xem log
idf.py -p /dev/ttyUSB0 flash monitor
```

## 8. Chạy Unit Test

```bash
idf.py -T test_parser build
idf.py -T test_parser flash monitor
```

---

## 9. Roadmap học & phát triển

Xem chi tiết lộ trình học FreeRTOS + ESP-IDF trước khi bắt tay vào dự án này tại [`ROADMAP.md`](./ROADMAP.md).

## 10. Hướng phát triển tiếp theo

- Thêm secure boot + flash encryption cho triển khai thương mại thật.
- Hỗ trợ nhiều cảm biến qua I2C multiplexer (giám sát nhiều điểm trong 1 kho).
- Dashboard xem lịch sử nhiệt độ theo hành trình vận chuyển (bản đồ + biểu đồ).
- Chuẩn hoá dữ liệu theo giao thức LWM2M hoặc chuẩn cold-chain (GS1) nếu hướng tới sản phẩm thương mại.

## License

MIT — điều chỉnh tuỳ theo mục đích sử dụng (cá nhân/học tập hay portfolio công khai).
