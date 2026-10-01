# Kết quả kiểm tra đổi GPIO — 27/09/2026

Đã chạy trên PC; chưa flash hoặc kiểm tra điện áp/hoạt động thật trên ESP32.

## Host

`powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/run_all.ps1` kết thúc mã 0.

| Nhóm | Kết quả |
| --- | --- |
| Core | 123 checks |
| Control runtime | 327 checks |
| Debounce / GPIO mặc định | 20 / 28 checks |
| Storage | 140 checks |
| Sensor mặc định | 234 checks |
| Display mặc định | 2957 checks |
| Gateway / network | 142 / 119 checks |
| UI Node | Đạt các kịch bản schema, ACK, slider conflict, save, reconnect, timeout, no replay |
| Profile Python | 4 test methods đạt, gồm giữ GPIO trong config/sdkconfig.board qua lần tạo profile tiếp theo |
| Pin policy | 32 ca compiler đạt: pin hợp lệ, ánh xạ sáu ADC1, pin không phù hợp, trùng chân |
| Driver bộ GPIO khác | GPIO 28, sensor 234, display 2957 checks đạt |

Bộ chân thử khác: MQ2=39, SDA=18, SCL=19, relay=4, nút=13/14/16/17. Ca âm tính chỉ được coi đạt khi compiler từ chối bằng static assertion đúng loại.

Một lần chạy ban đầu lỗi test do include board_pins.h ở cuối test_sensor.c; đã ghi log compiler, chuyển include lên đầu và chạy lại toàn bộ thành công. Không phải lỗi driver trên board.

## ESP-IDF 5.5.4

Build bằng `tests/build_firmware.ps1 -ThuBanGiao` với các đường dẫn toolchain đã cài tại máy phát triển, không flash. Profile bật phần cứng thật, Wi-Fi/web, dò LCD; dùng SSID/password fixture không phải thông tin mạng thật.

- Bộ mặc định: build exit 0, app `0xd3bd0` byte.
- Sửa cả tám CONFIG_GAS_GPIO_* trong sdkconfig.ready của build thử theo bộ trên rồi gọi lại cùng helper: build exit 0, app `0xd3bd0` byte. Bộ tạo profile đã giữ lại các giá trị mới.
- Phân vùng app `0x100000` byte; còn `0x2c430` byte (17%).

Sau khi ghi nhận kết quả, các thư mục build/cache/out được dọn theo yêu cầu người dùng. Binary/log gốc có thể tạo lại bằng runner; báo cáo này giữ kết quả đã quan sát trước khi dọn. Không xóa source test hoặc dữ liệu cấu hình cá nhân.
