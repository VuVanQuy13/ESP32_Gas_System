# Hệ thống giám sát khí gas bằng ESP32

## 1. Giới thiệu project và tính năng

Đây là project firmware sử dụng ESP-IDF cho board ESP32-DevKit V1. Hệ thống đọc cảm biến khí gas MQ-2 và cảm biến lửa dạng số, điều khiển relay, hiển thị trạng thái trên LCD1602 giao tiếp I2C, cung cấp giao diện web trong mạng Wi-Fi nội bộ và có thể gửi cảnh báo qua Telegram Bot.

### Tính năng chính

- Đọc ngõ ra analog AO của MQ-2 bằng ADC1 của ESP32.
- Lọc tối đa bốn mẫu ADC và loại bỏ mẫu không hợp lệ, quá cũ hoặc bị timeout.
- Giá trị MQ-2 hiện là số đếm ADC thô từ `0` đến `4095`, chưa chuyển đổi thành ppm đã hiệu chuẩn.
- Có hai chế độ điều khiển relay: Auto và Manual.
- Dùng hysteresis để giảm hiện tượng relay đóng/ngắt liên tục quanh ngưỡng gas.
- Khi ở Auto, nếu phát hiện lửa hợp lệ thì relay được yêu cầu bật.
- Khi ở Manual, relay chỉ được điều khiển bằng nút cứng hoặc lệnh từ web.
- Có bốn nút active-low: bật relay, tắt relay, chuyển Auto/Manual và chỉnh/lưu ngưỡng.
- Web và nút cứng cùng sử dụng một bản nháp ngưỡng trên ESP32.
- Lưu ngưỡng hiệu lực vào bộ nhớ NVS của ESP32.
- LCD hiển thị chế độ, relay, trạng thái gas, ngưỡng, trạng thái chỉnh sửa và trạng thái lửa.
- Có web dashboard dùng HTTP và WebSocket trong cùng mạng Wi-Fi.
- Có thể gửi cảnh báo gas và lửa qua Telegram bằng HTTPS.
- Ghi log trạng thái khởi động, cảm biến, nút, relay, mạng, lưu trữ, web và Telegram ra serial monitor.

### Logic hoạt động

Trong chế độ Auto, trạng thái lửa hợp lệ được ưu tiên. Nếu không có lửa, relay hoạt động theo hysteresis của MQ-2: bật khi ADC lớn hơn hoặc bằng ngưỡng và tắt khi ADC nhỏ hơn hoặc bằng `ngưỡng - hysteresis`. Dữ liệu MQ-2 không hợp lệ hoặc quá hạn sẽ không tạo yêu cầu bật relay theo gas.

Trong chế độ Manual, relay thực hiện theo lệnh bật/tắt từ nút cứng hoặc web. Hệ thống vẫn theo dõi gas và lửa để tạo cảnh báo Telegram, nhưng các cảnh báo này không tự thay đổi relay Manual.

Thông báo xác nhận hết lửa chỉ được gửi sau khi D0 báo không có lửa liên tục trong ba giây. Khoảng thời gian này chỉ trì hoãn thông báo, không trì hoãn phản ứng relay trong Auto.

## 2. Hardware và firmware

### Hardware

| Thiết bị | Kết nối mặc định với ESP32 | Ghi chú |
|---|---|---|
| ESP32-DevKit V1 | ESP32 classic | Các số GPIO là số GPIO của MCU. |
| MQ-2 AO | GPIO34 / ADC1_CH6 | Đọc ADC 12-bit thô. Điện áp vào ADC phải nằm trong giới hạn của ESP32. |
| Cảm biến lửa D0 | GPIO4 | Mặc định active-low; có thể đổi cực tính trong menuconfig. |
| LCD1602 với backpack PCF8574 | SDA GPIO21, SCL GPIO22 | I2C 100 kHz, địa chỉ mặc định `0x27`. |
| Ngõ vào relay | GPIO23 | Active-high: HIGH yêu cầu bật, LOW yêu cầu tắt. |
| Nút ON/+ | GPIO25 | Active-low, dùng pull-up nội bộ ESP32. |
| Nút OFF/- | GPIO26 | Active-low, dùng pull-up nội bộ ESP32. |
| Nút Auto/Manual | GPIO27 | Active-low, dùng pull-up nội bộ ESP32. |
| Nút Edit/Save | GPIO32 | Active-low, dùng pull-up nội bộ ESP32. |

Backpack LCD đã nối sẵn các chân điều khiển LCD với PCF8574. Driver hiện giả định P0=RS, P1=RW, P2=E, P3=backlight và P4..P7=D4..D7. Đây là các đường nối bên trong backpack, không phải các GPIO bổ sung của ESP32.


### Cấu trúc firmware

```text
MQ-2 ADC + flame D0 ─┐
Nút cứng ────────────┼─> các task ─> control_task ─> control_core
Lệnh WebSocket ──────┘                         ├─> GPIO relay
                                               ├─> snapshot LCD
                                               ├─> snapshot web
                                               ├─> storage_task -> NVS
                                               └─> Telegram worker
```

Điểm bắt đầu chương trình là `Gas_System/main/Gas_System.c`. File này đọc cấu hình menuconfig, khởi tạo driver phần cứng và NVS, tạo các FreeRTOS task, khởi động Wi-Fi và khởi động web service.

### Build và cấu hình

Mở terminal ESP-IDF 5.5.4 tại thư mục `Gas_System`:

```powershell
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Trong menu **Gas_System** của menuconfig có thời gian ổn định MQ-2, cấu hình LCD, hysteresis ADC thô, toàn bộ GPIO, cực tính cảm biến lửa, Wi-Fi, web và Telegram. Sau khi Wi-Fi nhận IP, monitor sẽ in URL dạng `http://<device-ip>` để truy cập web.

## 3. Kiến thức Embedded đã sử dụng

### Ngoại vi của MCU

- **ADC1 oneshot:** đọc AO của MQ-2 với độ phân giải 12-bit và cấu hình attenuation.
- **GPIO input/output:** đọc nút, đọc D0 cảm biến lửa và điều khiển relay.
- **GPIO interrupt:** ngắt nút và D0 chỉ đánh thức task; ISR không thực hiện xử lý phức tạp.
- **I2C master:** giao tiếp với backpack PCF8574 của LCD ở tốc độ 100 kHz.
- **UART0:** dùng cho serial monitor và log runtime.
- **Non-volatile storage:** NVS lưu ngưỡng sau khi ESP32 reset hoặc mất nguồn.

### RTOS và đồng bộ dữ liệu

- FreeRTOS task tách riêng việc đọc cảm biến, đọc nút, điều khiển, hiển thị, lưu trữ, mạng và thông báo.
- Queue giới hạn dùng để truyền event giữa các producer và control task.
- Task notification đánh thức task hiệu quả từ GPIO ISR.
- Mô hình single-owner giúp tránh race condition trên relay, mode, ngưỡng và trạng thái cảnh báo.
- LCD và web đọc snapshot, không tự sửa state điều khiển.
- HTTP/Telegram chạy ngoài đường quyết định relay nên mạng chậm không chặn control task.

### Kỹ thuật lập trình Embedded

- State machine hướng event với các trạng thái Auto/Manual, cảm biến, chỉnh sửa, lưu và cảnh báo.
- Hysteresis để giảm relay chattering gần ngưỡng gas.
- Debounce nút và xử lý input theo cạnh ngắt.
- Điều kiện warm-up và số lượng mẫu hợp lệ liên tiếp cho MQ-2.
- Xử lý timeout, dữ liệu cũ, ADC ngoài miền, queue đầy và lỗi phần cứng.
- Lưu NVS bất đồng bộ với save ID và kết quả giao dịch.
- WebSocket snapshot có version; revision và boot ID giúp từ chối lệnh web cũ.
- Cấu hình board bằng Kconfig và kiểm tra GPIO hợp lệ/trùng nhau khi build.
- CMake component giúp tách driver, task, control logic, storage, network, web và Telegram.
- Structured logging để quan sát hệ thống và host test xác định cho control, debounce và format Telegram.


## 4. Hình ảnh project

Các ảnh được đặt trong thư mục `images` của repository:

### Giao diện web

![Giao diện web](images/Web.png)

### Thông báo Telegram

![Thông báo Telegram](images/telegram.jpg)

### Sản phẩm thực tế

![Sản phẩm thực tế](images/Product.jpg)

