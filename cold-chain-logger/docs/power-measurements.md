# Đo lường tiêu thụ năng lượng

## Phương pháp đo

- **Thiết bị**: Multimeter / INA219 module đo dòng inline
- **Điểm đo**: Giữa pin Li-ion và input TP4056 hoặc trực tiếp trên 3V3 rail

## Kết quả đo (TODO — điền sau khi có phần cứng)

| Chế độ | Dòng tiêu thụ | Ghi chú |
|---|---|---|
| Active (đọc cảm biến + Wi-Fi) | ___ mA | CPU 240MHz, Wi-Fi connected |
| Active (đọc cảm biến, không Wi-Fi) | ___ mA | CPU 240MHz, Wi-Fi off |
| Light sleep | ___ mA | Giữa các chu kỳ đo ngắn |
| Deep sleep | ___ µA | Giữa các chu kỳ đo dài |
| Deep sleep + RTC | ___ µA | Với DS3231 RTC ngoài |

## Tính toán thời lượng pin

- Pin 18650: ~3000 mAh
- Chu kỳ đo: mỗi 60 giây
- Thời gian active mỗi chu kỳ: ~2 giây (đo) + ~5 giây (gửi MQTT nếu có mạng)
- Thời gian deep sleep: ~53 giây

```
Dòng trung bình = (I_active × T_active + I_sleep × T_sleep) / T_total
Thời lượng = Capacity / I_avg
```

## Tối ưu đã áp dụng

- [ ] Deep sleep giữa các chu kỳ đo
- [ ] Tắt Wi-Fi khi không cần gửi dữ liệu
- [ ] Giảm CPU freq khi idle (Power Management)
- [ ] Tickless idle (FreeRTOS)
- [ ] BME280 forced mode (không để continuous)

