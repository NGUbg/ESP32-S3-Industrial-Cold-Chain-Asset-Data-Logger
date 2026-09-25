# ESP32-S3 Industrial Cold-Chain & Asset Data Logger

> Firmware-first IoT data logger cho bài toán giám sát chuỗi lạnh (cold-chain) / tài sản di động — thiết kế theo đúng các ràng buộc kỹ thuật mà một hệ thống nhúng chạy thật ngoài sản xuất cần có: đa nhiệm thời gian thực (RTOS), hoạt động ổn định khi mất mạng, bảo mật truyền thông, cập nhật OTA an toàn, và tối ưu năng lượng.

Một firmware nhúng chạy trên ESP32-S3, dùng ESP-IDF thuần, đóng vai trò thiết bị ghi log nhiệt độ/độ ẩm cho bài toán cold-chain (chuỗi lạnh) — ví dụ theo dõi dược phẩm, thực phẩm đông lạnh, thiết bị y tế trong quá trình vận chuyển/lưu kho. 

> **Lưu ý phạm vi**: đây là một dự án học tập/portfolio quy mô lớn, không phải firmware sẵn sàng thương mại. Dự án được chia thành 5 phase độc lập — mỗi phase cho ra một firmware **chạy được và demo được**, thay vì cố hoàn thành toàn bộ checklist cùng lúc rồi không có gì chạy trọn vẹn.

---

## 1. Bài toán

Trong vận chuyển/lưu kho hàng nhạy cảm với nhiệt độ (dược phẩm, thực phẩm đông lạnh, thiết bị y tế), cần một thiết bị:
- Ghi lại nhiệt độ/độ ẩm liên tục, kể cả khi **không có kết nối mạng** trong thời gian dài (xe tải di chuyển, kho không có Wi-Fi).
- Tự động **đồng bộ dữ liệu** khi có kết nối trở lại, không được mất dữ liệu đã ghi.
- **Cảnh báo ngay** khi vượt ngưỡng an toàn — kể cả khi đang mất mạng (cảnh báo cục bộ) lẫn khi có mạng (cảnh báo từ xa).
- Chạy được **nhiều tháng bằng pin**.
- Có thể **cập nhật firmware từ xa** khi triển khai số lượng lớn (không thể thu hồi thiết bị để nạp lại bằng tay), và **không cho phép firmware giả mạo được cài vào thiết bị**.

## 2. Vì sao chọn bài toán này để luyện kỹ năng

| Yêu cầu bài toán | Kỹ năng firmware tương ứng |
|---|---|
| Ghi liên tục kể cả mất mạng | Store-and-forward, file system nhúng (LittleFS) |
| Không mất dữ liệu khi crash/mất điện | NVS, watchdog, brown-out detection |
| Đồng thời đọc cảm biến + quản lý lưu trữ + mạng + cảnh báo | FreeRTOS đa task, Queue, Mutex, Event Group |
| Timestamp chính xác cho dữ liệu cold-chain | SNTP + RTC dự phòng khi deep sleep dài |
| Chạy nhiều tháng bằng pin | Deep sleep, đo & tối ưu dòng tiêu thụ theo ngân sách mục tiêu |
| Cập nhật số lượng lớn thiết bị từ xa, không cho phép firmware giả | OTA qua HTTPS, signature verification, cơ chế rollback |
| Dữ liệu nhạy cảm khi truyền qua mạng công cộng | MQTT qua TLS (mTLS per-device), không anonymous |
| Cấp identity cho từng thiết bị khi sản xuất hàng loạt | Certificate provisioning, BLE provisioning |
| Độ tin cậy khi vận hành vô nhân sự | Unit test, CI, error handling có hệ thống |

---

## 3. Kiến trúc hệ thống

```
                    ┌──────────────────────────────────────────────────┐
                    │                 ESP32-S3 (FreeRTOS)                │
                    │                                                    │
  Cảm biến I2C  ──▶ │  [sensor_task] ──Queue──▶ [storage_task]           │
  (nhiệt độ/ẩm)     │        │                       │      │            │
                    │        │                  LittleFS   │            │
                    │        │                  (lưu khi   │            │
                    │        │                  mất mạng)  │            │
                    │        ▼                             ▼            │
                    │  [alert_task] ◀──Queue── (ngưỡng vượt)  Queue      │
                    │   │        │                             │        │
                    │   ▼        ▼                             ▼        │
                    │ LED/Buzzer  MQTT alert topic       [network_task] │
                    │ (offline)   (khi có mạng, QoS1)      │      │     │
                    │                                       │      │    │
                    │  [health_task]                        │      │    │
                    │  (watchdog, pin, brown-out,            │      │    │
                    │   SNTP time sync)                      ▼      ▼    │
                    └─────────────────────────────────MQTT/TLS  OTA/HTTPS┘
                                                          │        │
                                                          ▼        ▼
                                                 Broker/Backend  Update server
                                                 (mTLS per-device, (signed image,
                                                  QoS1)             rollback)
```

5 task chính, giao tiếp qua FreeRTOS Queue/Event Group (không dùng biến toàn cục chia sẻ trực tiếp):

| Task | Vai trò |
|---|---|
| `sensor_task` | Đọc cảm biến I2C định kỳ, gắn timestamp, đẩy bản ghi vào Queue, kiểm tra ngưỡng và báo `alert_task` |
| `storage_task` | Nhận bản ghi, ghi vào LittleFS nếu chưa có mạng, đánh dấu đã gửi khi thành công, quản lý ring-buffer khi đầy |
| `alert_task` | Nhận sự kiện vượt ngưỡng, kích hoạt LED/buzzer cục bộ ngay lập tức, publish alert (QoS1, retain) khi có mạng |
| `network_task` | Kết nối Wi-Fi/MQTT qua TLS (mTLS), gửi dữ liệu tồn đọng + alert khi có mạng, nhận lệnh OTA |
| `health_task` | Feed watchdog, theo dõi điện áp pin, đồng bộ thời gian qua SNTP khi có mạng, log trạng thái hệ thống định kỳ |

> Alert phải **không phụ thuộc vào mạng**: `alert_task` kích hoạt cảnh báo cục bộ ngay khi nhận sự kiện, độc lập với trạng thái kết nối; việc gửi alert qua MQTT chỉ là kênh bổ sung khi có mạng.

---

## 4. Yêu cầu kỹ thuật bắt buộc (Definition of Done)

Chia theo phase — xem chi tiết thứ tự thực hiện tại [`ROADMAP.md`](./ROADMAP.md).

**Phase 1 — Core RTOS & offline-first**
- [ ] 100% viết bằng **ESP-IDF thuần** (không Arduino framework).
- [ ] Tối thiểu **5 FreeRTOS task** như kiến trúc ở trên, giao tiếp qua Queue/Event Group, có priority và stack size được tính toán và ghi lại lý do (không dùng giá trị mặc định tùy tiện).
- [ ] **Store-and-forward**: dữ liệu không bị mất khi mất mạng; chính sách khi LittleFS đầy (ring-buffer ghi đè bản cũ nhất) được cài đặt và test.
- [ ] Định dạng dữ liệu lưu/truyền được quyết định rõ ràng (xem mục 5) kèm lý do chọn.
- [ ] **Cảnh báo cục bộ** (LED/buzzer) hoạt động độc lập với trạng thái mạng.

**Phase 2 — Time & connectivity**
- [ ] **SNTP time sync** khi có mạng; RTC ngoài (DS3231) dự phòng khi deep sleep dài không có mạng — bắt buộc, không còn là tùy chọn.
- [ ] **MQTT qua TLS**, dùng **mTLS per-device** (mỗi thiết bị một cert/key riêng), không dùng kết nối anonymous.
- [ ] Alert được publish qua MQTT (QoS1) khi có mạng.

**Phase 3 — Resilience**
- [ ] **Watchdog timer** + xử lý brown-out, tự phục hồi sau khi crash (test bằng cách chủ động gây lỗi/reset, log lại thời gian phục hồi).
- [ ] **Unit test** (Unity) cho toàn bộ logic thuần (parser, ngưỡng cảnh báo, ring-buffer...), chạy được trong CI không cần phần cứng thật.

**Phase 4 — Secure OTA**
- [ ] **OTA update qua HTTPS**, firmware image **có ký số (signed)** và được xác minh trước khi flash.
- [ ] Cơ chế **rollback tự động** nếu firmware mới lỗi/không xác nhận được (app rollback của ESP-IDF, đánh dấu "valid" sau self-test).
- [ ] Ghi rõ mô hình đe dọa (threat model) tối thiểu cho OTA: ai có thể đẩy firmware, làm sao thiết bị xác minh nguồn.

**Phase 5 — Power & provisioning ở quy mô**
- [ ] **Deep sleep** giữa các chu kỳ đo, có **ngân sách năng lượng mục tiêu** đặt trước (µA lúc sleep, mA lúc active, ước tính tuổi thọ pin) và số liệu đo thực tế trước/sau tối ưu để so sánh (xem mục 6).
- [ ] **BLE Provisioning** để cấu hình Wi-Fi, không hard-code SSID/password trong source code.
- [ ] Quy trình cấp cert/identity riêng cho từng thiết bị lúc "sản xuất" (kể cả mô phỏng bằng script) — nền tảng để triển khai số lượng lớn thật.

**Tài liệu (xuyên suốt các phase)**
- [ ] Sơ đồ kiến trúc, state diagram cho storage/network/alert.
- [ ] Bảng số liệu tiêu thụ năng lượng (mục tiêu vs đo thực tế).
- [ ] Ghi lại các quyết định thiết kế và đánh đổi (design decisions & trade-offs) — không chỉ code, mà cả lý do.

---

## 5. Quyết định thiết kế dữ liệu (cần chốt trước khi code Phase 1)

| Vấn đề | Quyết định | Lý do |
|---|---|---|
| Format bản ghi lưu/truyền | CBOR (nhị phân, gọn) thay vì JSON | Thiết bị chạy pin, băng thông MQTT tốn năng lượng — CBOR nhỏ hơn JSON đáng kể cho cùng dữ liệu |
| Chính sách khi LittleFS đầy | Ring-buffer: ghi đè bản ghi cũ nhất chưa gửi | Ưu tiên dữ liệu gần nhất cho cảnh báo/giám sát hơn là dừng ghi hoàn toàn |
| Chu kỳ đo mặc định | Cấu hình được qua Kconfig/NVS, mặc định 60s | Cân bằng giữa độ chi tiết dữ liệu và tuổi thọ pin, tùy use-case cho phép chỉnh |
| Flash wear của LittleFS | Đánh giá số chu kỳ ghi/xóa dự kiến trong 1 chuyến vận chuyển dài nhất, đối chiếu với wear-leveling của LittleFS | Ghi liên tục nhiều tháng khi mất mạng có thể ảnh hưởng tuổi thọ flash nếu không tính trước |

---

## 6. Ngân sách năng lượng (mục tiêu — cập nhật số liệu thật khi đo)

| Trạng thái | Dòng tiêu thụ mục tiêu | Ghi chú |
|---|---|---|
| Deep sleep | ≤ 20 µA | Bao gồm RTC ngoài nếu dùng |
| Active — đọc cảm biến | < 20 mA, trong ~100ms | I2C read, không bật radio |
| Active — Wi-Fi/MQTT publish | < 150 mA, trong vài giây | Chỉ bật khi có dữ liệu cần gửi hoặc theo chu kỳ đồng bộ |
| Mục tiêu tuổi thọ pin (18650, 3000mAh) | ≥ 3 tháng ở chu kỳ đo 60s, sync mỗi giờ | Tính toán lại sau khi có số đo thực tế Phase 5 |

> Số liệu ở trên là **mục tiêu ban đầu để thiết kế**, không phải cam kết — mục DoD Phase 5 yêu cầu đo thực tế và so sánh, không chỉ đo cho có.

---

## 7. Phần cứng

| Thành phần | Vai trò |
|---|---|
| ESP32-S3-DevKitC-1 (PSRAM) | MCU chính, chạy FreeRTOS |
| Cảm biến nhiệt độ/độ ẩm I2C (BME280 hoặc SHT31) | Nguồn dữ liệu chính — dùng driver component có sẵn (ESP Component Registry) thay vì tự viết từ đầu, để dồn effort cho phần lõi RTOS/bảo mật/năng lượng |
| Pin Li-ion 18650 + module sạc/bảo vệ (TP4056) | Nguồn nuôi độc lập, đo thời lượng hoạt động |
| Module đo điện áp pin (ADC chia áp) | Theo dõi mức pin cho `health_task` |
| Module RTC ngoài (DS3231) | **Bắt buộc** (không còn tùy chọn) — giữ thời gian chính xác khi deep sleep dài không có mạng để sync SNTP |
| LED/Buzzer | Cảnh báo cục bộ khi vượt ngưỡng, độc lập với mạng |

---

## 8. Cấu trúc thư mục dự kiến

```
cold-chain-logger/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── app_main.c              # khởi tạo task, queue, event group
│   ├── sensor_task.c/.h
│   ├── storage_task.c/.h
│   ├── alert_task.c/.h
│   ├── network_task.c/.h
│   └── health_task.c/.h
├── components/
│   ├── storage_manager/        # wrapper LittleFS + ring-buffer + logic store-and-forward
│   ├── record_codec/           # encode/decode CBOR cho bản ghi (logic thuần, test được)
│   └── ota_manager/            # xác minh chữ ký + rollback
├── test/
│   ├── test_record_codec.c     # unit test CBOR encode/decode
│   ├── test_alert_threshold.c  # unit test logic ngưỡng cảnh báo
│   └── test_ring_buffer.c      # unit test chính sách ghi đè khi đầy
├── docs/
│   ├── architecture.md
│   ├── power-measurements.md
│   ├── security-model.md       # threat model OTA + mTLS
│   └── state-diagram.png
└── .github/workflows/build.yml # CI build + unit test tự động
```

---

## 9. Build & Flash

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

## 10. Chạy Unit Test

```bash
idf.py -T test_record_codec build
idf.py -T test_record_codec flash monitor
```

---

## 11. Roadmap học & phát triển

Dự án được chia thành **5 phase**, mỗi phase cho ra firmware chạy/demo được. Xem chi tiết lộ trình học FreeRTOS + ESP-IDF và thứ tự triển khai từng phase tại [`ROADMAP.md`](./ROADMAP.md).

## 12. Hướng phát triển tiếp theo (sau khi hoàn thành 5 phase)

- Flash encryption cho triển khai thương mại thật (secure boot đã đưa vào Phase 4 ở trên).
- Hỗ trợ nhiều cảm biến qua I2C multiplexer (giám sát nhiều điểm trong 1 kho).
- Dashboard xem lịch sử nhiệt độ theo hành trình vận chuyển (bản đồ + biểu đồ).
- Chuẩn hoá dữ liệu theo giao thức LWM2M hoặc chuẩn cold-chain (GS1) nếu hướng tới sản phẩm thương mại.
- Cấp phát certificate tự động ở quy mô sản xuất thật (hiện tại Phase 5 chỉ mô phỏng bằng script).

## License

MIT — điều chỉnh tuỳ theo mục đích sử dụng (cá nhân/học tập hay portfolio công khai).
