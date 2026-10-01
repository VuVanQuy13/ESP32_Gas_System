# Đo khí gas với ESP32, MQ-2, LCD và web LAN

Firmware ESP-IDF 5.5.4 cho ESP32-DevKit V1 (ESP32 classic). Các module điều khiển, nút, relay, ADC MQ-2, LCD PCF8574, NVS và web đã được tích hợp. Phần mềm có kiểm thử host; chưa nạp/đo trên mạch thật.

**Bắt đầu tại [hướng dẫn cấu hình và nạp](docs/flash-guide.md).** File cấu hình cá nhân cho phép nhập Wi-Fi, thời gian ổn định MQ-2 và cấu hình LCD; không cần sửa source C.

- [Nối mạch](docs/wiring.md)
- [Đổi chân bằng menuconfig](docs/gpio-menuconfig.md)
- [Đọc source toàn dự án](docs/codebase-map.md)
- [Core điều khiển](docs/code-walkthrough.md)
- [Task điều khiển và queue](docs/control-task-guide.md)
- [Nút và relay](docs/button-task-guide.md)
- [ADC và cảm biến](docs/sensor-guide.md)
- [LCD](docs/display-guide.md)
- [Lưu NVS](docs/storage-guide.md)
- [Wi-Fi/web](docs/web-guide.md)

Chạy toàn bộ kiểm thử phần mềm tại thư mục gốc:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/run_all.ps1
```

Cần GCC host, Python, Node.js và ESP-IDF cho cJSON. Nhật ký ở thư mục `out` của từng nhóm trong `Gas_System/tests`.

Phép đo hiện là ADC thô, không phải ppm. Đây là project demo với tải LED. Các ngưỡng và kiểm tra tín hiệu cần được xác nhận trên mạch cuối cùng.
