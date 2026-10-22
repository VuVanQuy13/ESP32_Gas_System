# Hệ thống giám sát khí gas với ESP32 và MQ-2

Project firmware nhúng dùng **ESP32-DevKit V1** và **ESP-IDF 5.5.4**. Hệ thống đọc tín hiệu analog từ module MQ-2, hiển thị trạng thái trên LCD 16×2, điều khiển một relay cho tải LED demo và cung cấp giao diện web trong mạng Wi-Fi nội bộ.

> Đây là project học tập/demo. Firmware hiện xử lý **giá trị ADC thô**, chưa chuyển đổi hoặc hiệu chuẩn thành nồng độ ppm; không dùng như thiết bị cảnh báo an toàn khí gas.

## Tính năng

- Đọc ngõ **AO** của MQ-2 định kỳ bằng ADC1; kiểm tra mẫu và chờ thời gian ổn định đã cấu hình trước khi báo cảm biến sẵn sàng.
- Đọc ngõ **D0** của module phát hiện lửa qua GPIO4 mặc định; polarity LOW/HIGH có thể chọn trong menuconfig. Trong Auto, phát hiện lửa bật relay độc lập với số đo MQ-2.
- Điều khiển hệ thống ở hai chế độ:
  - **Auto:** relay bật khi gas vượt ngưỡng **hoặc** phát hiện lửa; tắt theo hysteresis gas khi không còn lửa.
  - **Manual:** relay chỉ thay đổi theo nút cứng hoặc lệnh web; trạng thái vượt ngưỡng vẫn được hiển thị.
- Hiển thị giá trị cảm biến, chế độ, relay và trạng thái hệ thống trên LCD 16×2.
- Có bốn nút cứng: bật/tăng, tắt/giảm, đổi Auto/Manual và vào/lưu chỉnh ngưỡng.
- Cung cấp trang web qua Wi-Fi cùng mạng để xem trạng thái thời gian thực, điều khiển relay/chế độ và chỉnh ngưỡng.
- Lưu ngưỡng vào NVS; khi khởi động lại hệ thống dùng ngưỡng đã lưu, bắt đầu ở Auto và giữ relay tắt cho đến khi cảm biến sẵn sàng.
- Khi mất dữ liệu cảm biến hợp lệ trong lúc chạy Auto, hệ thống tắt relay và báo lỗi cảm biến.
- Có thể gửi Telegram khi gas vượt ngưỡng/hồi phục hoặc lửa được phát hiện/xác nhận đã hết. Sự kiện gas và lửa cùng lúc được gộp thành một tin nhắn; lỗi đọc D0 không được xem là xác nhận hết lửa.
- Cho phép đổi GPIO từ `menuconfig`; firmware kiểm tra một số chân không phù hợp hoặc bị trùng khi biên dịch.

## Phần cứng

| Thành phần | Sử dụng trong project |
| --- | --- |
| ESP32-DevKit V1 (ESP32 classic) | MCU chạy firmware và kết nối Wi-Fi |
| Module MQ-2 / FC-22 có AO/DO | Firmware đọc **AO**; chân DO không dùng |
| Module cảm biến lửa có ngõ D0 | Firmware đọc **D0** bằng GPIO4 mặc định; cần xác nhận cực tính và mức điện áp D0 của module |
| LCD 16×2 kèm backpack PCF8574 | Hiển thị qua bus I2C |
| 4 nút nhấn | Nối GPIO xuống GND khi nhấn; cấu hình hiện tại dùng pull-up nội bộ |
| Module relay 5 V, 10 A | Firmware điều khiển ngõ logic active-high: HIGH bật, LOW tắt |
| LED rời và điện trở hạn dòng | Tải demo 3,3 V được đóng/ngắt qua tiếp điểm relay |
| Nguồn và linh kiện giao tiếp | Theo sơ đồ nối mạch; MQ-2/LCD dùng nhánh 5 V ngoài, chung GND với ESP32. Sơ đồ LCD 5 V có bộ chuyển mức I2C hai chiều |

GPIO mặc định trong `sdkconfig`: MQ-2 AO = GPIO34, cảm biến lửa D0 = GPIO4, LCD SDA/SCL = GPIO21/GPIO22, relay = GPIO23, bốn nút = GPIO25/GPIO26/GPIO27/GPIO32. Có thể thay đổi trong `idf.py menuconfig` → `Gas_System` → `Chan GPIO` và phải đấu dây theo cấu hình mới. D0 phải giữ mức điện áp tương thích ESP32 (khuyến nghị không quá 3,3 V).

## Phần mềm và công cụ

- **ESP-IDF 5.5.4** và toolchain tương ứng.
- **C** cho firmware; **FreeRTOS** cho các task và giao tiếp giữa task.
- **CMake** để khai báo project/component; **Kconfig/menuconfig** để cấu hình GPIO, LCD, thời gian ổn định MQ-2, Wi-Fi và web.
- **ESP-IDF HTTP Server + WebSocket** phục vụ trang web và cập nhật trạng thái/lệnh trong LAN.
- **HTML, CSS, JavaScript** cho giao diện web.
- Tài liệu yêu cầu và ma trận kiểm thử nằm trong `docs/test-matrix.md`; mã chạy kiểm thử host không có trong cây source hiện tại.

## MCU peripherals và giao tiếp được sử dụng

| Peripheral/giao tiếp | Vai trò |
| --- | --- |
| **GPIO input** | Đọc bốn nút nhấn; task nút lọc dội (debounce) bằng phần mềm |
| **GPIO input** | Đọc D0 của cảm biến lửa theo chu kỳ sensor task; log trạng thái khi đổi |
| **GPIO output** | Xuất mức điều khiển relay; LOW được đặt khi khởi tạo để relay bắt đầu tắt |
| **ADC1, oneshot** | Đọc analog từ MQ-2 AO. Cấu hình hiện tại dùng độ phân giải 12-bit và attenuation 12 dB; giá trị phần mềm dùng là ADC thô |
| **I2C master** | Giao tiếp với PCF8574 của LCD ở 100 kHz; địa chỉ có thể cấu hình hoặc dò trong dải PCF8574/PCF8574A |
| **UART0** | Cổng console/serial monitor mặc định, baudrate 115200 để xem log ESP-IDF |
| **Wi-Fi station (STA)** | Kết nối router Wi-Fi; ESP32 nhận địa chỉ IP để truy cập web trong cùng mạng |

Các dịch vụ ESP-IDF đi cùng nhưng không phải chân ngoại vi nối trực tiếp: **FreeRTOS** (task/queue), **esp_timer** (mốc thời gian và thử kết nối lại), **NVS** (lưu ngưỡng), **event loop/netif** (sự kiện Wi-Fi và IP), **HTTP Server/WebSocket** (web LAN).

## Bắt đầu

1. Cài và mở môi trường ESP-IDF 5.5.4.
2. Đọc [sơ đồ nối mạch](docs/wiring.md) và kiểm tra mức điện áp trước khi nối AO vào ESP32.
3. Từ thư mục `Gas_System`, chạy `idf.py menuconfig`; vào `Gas_System` để cấu hình Wi-Fi, thời gian ổn định MQ-2, địa chỉ LCD, GPIO, hysteresis và Telegram theo mạch thực tế. Hysteresis mặc định 50 là số đếm ADC thô, không phải ppm. Không commit mật khẩu Wi-Fi hoặc token Telegram.
4. Build và nạp firmware bằng lệnh dưới đây; thay `COMx` bằng cổng serial của board:

   ```powershell
   idf.py set-target esp32
   idf.py build
   idf.py -p COMx flash monitor
   ```

5. Serial monitor chạy ở 115200 baud. Sau khi ESP32 kết nối Wi-Fi, tìm log `url=http://...` rồi mở địa chỉ đó trên thiết bị cùng mạng.

Hướng dẫn chi tiết: [đổi GPIO](docs/gpio-menuconfig.md), [cấu hình và kiểm tra Telegram Bot](docs/telegram-guide.md), [bản đồ source](docs/codebase-map.md), [core điều khiển](docs/code-walkthrough.md), [task điều khiển và queue](docs/control-task-guide.md), [nút và relay](docs/button-task-guide.md), [ADC/MQ-2](docs/sensor-guide.md), [LCD](docs/display-guide.md), [NVS](docs/storage-guide.md), [Wi-Fi/web](docs/web-guide.md).

## Kiểm thử

[Ma trận kiểm thử](docs/test-matrix.md) ghi lại các trường hợp cần kiểm tra và giới hạn của kết quả đã có. Cây source hiện tại không chứa thư mục `Gas_System/tests`, nên không có lệnh chạy test tự động được cung cấp ở đây. Build thành công cũng không thay thế kiểm tra điện áp, nối dây và chạy thử trên board.

## Giới hạn hiện tại

- Ngưỡng đang áp dụng cho ADC thô, phụ thuộc module MQ-2, mạch chia áp, nguồn và từng board; cần hiệu chỉnh bằng số đo thực tế.
- Thời gian ổn định MQ-2 phải được xác định theo module và quá trình sử dụng; giá trị menuconfig không thay cho quá trình gia nhiệt/hiệu chuẩn cảm biến.
- Project được thiết kế cho tải demo LED. Chưa khẳng định phần cứng đã được nghiệm thu hoặc hiệu chuẩn như một thiết bị an toàn.
