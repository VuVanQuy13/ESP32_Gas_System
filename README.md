# Giám sát khí gas và phát hiện lửa với ESP32

Firmware ESP-IDF cho ESP32-DevKit V1 (ESP32 classic). Project đọc tín hiệu analog MQ-2 và tín hiệu số D0 của cảm biến lửa, điều khiển relay theo Auto/Manual, hiển thị trên LCD1602, cung cấp giao diện web trong mạng LAN và có thể gửi cảnh báo Telegram.

> Đây là project học tập và demo. MQ-2 hiện cung cấp **ADC thô**, chưa được hiệu chuẩn thành ppm. Nhãn `ppm` đang xuất hiện trong giao diện web và nội dung Telegram nhưng giá trị thực tế vẫn là ADC count; không dùng con số đó như nồng độ khí đã đo hoặc thiết bị an toàn.

## Tính năng

- Đọc MQ-2 AO bằng ADC1 12-bit, lọc trung bình tối đa 4 mẫu và loại mẫu lỗi/cũ. Task chờ khoảng 100 ms sau mỗi lượt đọc; cạnh thay đổi của D0 có thể đánh thức task sớm.
- Đọc D0 cảm biến lửa bằng GPIO. Mặc định LOW nghĩa là phát hiện lửa; có thể đổi cực tính bằng menuconfig. GPIO interrupt chỉ đánh thức sensor task; task chờ ổn định 30 ms rồi đọc lại D0.
- **Auto:** lửa hợp lệ yêu cầu bật relay ngay, kể cả khi MQ-2 chưa Ready. Khi không phát hiện lửa, luật gas quyết định: bật ở ngưỡng hoặc cao hơn, tắt ở `ngưỡng - hysteresis`, giữ trạng thái trong vùng giữa.
- **Manual:** relay theo nút cứng hoặc lệnh web. Cảnh báo gas/lửa vẫn được theo dõi và có thể gửi Telegram; các cảnh báo này không tự đổi relay trong Manual.
- Cần thời gian ổn định MQ-2 đã cấu hình và 3 mẫu ADC hợp lệ liên tiếp để chuyển sang Ready. Dữ liệu không hợp lệ/hết hạn khóa Auto theo luật gas.
- Bốn nút: ON/tăng, OFF/giảm, Auto/Manual và chỉnh/lưu ngưỡng. Khi chỉnh, nút +/- thay đổi bản nháp mỗi lần 10 ADC count; nút cứng và web dùng chung một bản nháp.
- Lưu ngưỡng vào NVS. Chỉ sau khi ghi thành công bản nháp mới thành ngưỡng hiệu lực. Sau reset, firmware khởi động Auto với relay OFF.
- Telegram thông báo khi gas vượt ngưỡng/hồi phục và khi phát hiện lửa/xác nhận hết lửa. Hồi phục gas cần 3 mẫu hợp lệ liên tiếp ở dưới `ngưỡng - hysteresis`; hết lửa cần D0 báo không lửa hợp lệ liên tục 3 giây. Lỗi đọc D0 không được xem là đã hết lửa.
- Web hiển thị số đo, chế độ, relay, ngưỡng và trạng thái D0; cho phép điều khiển, chỉnh/lưu ngưỡng. Web không trực tiếp điều khiển GPIO.

## Phần cứng

| Thành phần | Kết nối mặc định | Ghi chú |
|---|---|---|
| ESP32-DevKit V1, ESP32 classic | — | Firmware ESP-IDF; các chân trong bảng là số GPIO, không phải số thứ tự header |
| MQ-2 / FC-22, AO | GPIO34 / ADC1_CH6 | AO qua cầu chia áp 22 kΩ nối tiếp và 10 kΩ xuống GND, kèm tụ lọc 100 nF theo sơ đồ |
| Module cảm biến lửa, D0 | GPIO4 | Input số; cực tính active-low mặc định. Đảm bảo D0 không đưa quá 3,3 V vào ESP32 |
| LCD1602 + backpack PCF8574 | SDA GPIO21, SCL GPIO22 | I2C 100 kHz; địa chỉ mặc định 0x27; mapping backpack hiện giả định P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7 |
| Relay | IN GPIO23 | Tín hiệu active-high: HIGH yêu cầu bật, LOW yêu cầu tắt; trạng thái hiển thị là lệnh GPIO, không phải phản hồi tiếp điểm |
| Nút ON/+ và OFF/− | GPIO25 và GPIO26 | Input pull-up; mỗi nút nối GPIO xuống GND khi nhấn |
| Nút Auto/Manual và Edit/Save | GPIO27 và GPIO32 | Input pull-up; mỗi nút nối GPIO xuống GND khi nhấn |
| Nguồn | MQ-2/LCD dùng nhánh 5 V theo sơ đồ; ESP32 cấp USB | Nối GND chung. Không nối AO 5 V trực tiếp vào ESP32; kiểm tra mức D0 và chuyển mức I2C trước khi đấu |

Các GPIO có thể đổi trong `idf.py menuconfig` → `Gas_System` → `Chan GPIO`. Build kiểm tra chân hợp lệ và không trùng nhau. GPIO34–39 chỉ là input và không có pull-up/pull-down nội bộ; nếu chọn các chân này cho D0 phải đảm bảo module tạo mức logic ổn định bên ngoài.

Xem [sơ đồ nối dây](docs/wiring.md) và [hướng dẫn đổi GPIO](docs/gpio-menuconfig.md) trước khi lắp. Thông số phần cứng trong sơ đồ là thiết kế đề xuất; chưa có xác nhận đo điện áp hoặc nghiệm thu toàn mạch.

## Phần mềm và cấu trúc source

Project dùng **C, ESP-IDF 5.5.4, FreeRTOS, CMake và Kconfig/menuconfig**. Entry point là `app_main()` trong `Gas_System/main/Gas_System.c`.

```text
MQ-2 ADC ─┐
Flame D0 ─┴→ sensor_task ─┐
Nút nhấn ───→ button_task ├→ control_task → control_core
WebSocket ────────────────┘                    ├→ relay GPIO
                                               ├→ snapshot → LCD/web
                                               ├→ storage_task → NVS
                                               └→ Telegram worker
```

| Module | Trách nhiệm |
|---|---|
| `main/Gas_System.c` | Composition root: cấu hình, khởi tạo driver/NVS và tạo các task/service |
| `components/control_core` | State machine C thuần: Auto/Manual, readiness, hysteresis, chỉnh ngưỡng và các chuyển trạng thái cảnh báo |
| `components/app_tasks/control_task` | Chủ sở hữu duy nhất của trạng thái runtime; tuần tự hóa event, gọi core, xuất relay, tạo save job và phát snapshot |
| `components/app_tasks/sensor_task` | Đọc MQ-2 và D0, lọc ADC, giữ sequence/timestamp và gửi event tới control task |
| `components/app_tasks/button_task` | Đọc nút, debounce 30 ms, phát event sau cạnh nhấn ổn định |
| `components/app_tasks/storage_task` + `components/storage` | Thực hiện giao dịch NVS riêng và trả kết quả có liên kết save ID |
| `components/app_tasks/display_task` + `components/drivers/lcd1602` | Định dạng LCD, giao thức HD44780 4-bit và adapter PCF8574/I2C |
| `components/drivers` | Driver MQ-2 ADC, D0, GPIO nút và relay |
| `components/network` | Wi-Fi Station, xử lý sự kiện IP/mất kết nối và thử nối lại |
| `components/web_service` | HTTP server, WebSocket gateway và tài nguyên HTML/CSS/JavaScript nhúng trong firmware |
| `components/telegram_notify` | Worker HTTPS gửi Telegram, định dạng thông báo và giữ tối đa một sự kiện hết lửa đang chờ |

Điểm thiết kế chính là **single-owner state**: task cảm biến, nút, web và storage gửi event qua API/queue; chỉ `control_task` sửa trạng thái chung. LCD và web đọc hai mailbox snapshot độc lập. Giao tiếp mạng không chạy trong control task nên chờ HTTP không chặn quyết định relay.

## Giao thức và ngoại vi

### Trên ESP32

- **ADC1 oneshot:** MQ-2 AO, 12 bit, attenuation 12 dB; giá trị từ 0 đến 4095 là số đếm ADC. Bộ lọc trung bình tối đa 4 mẫu được xóa khi ADC lỗi, chạm rail 0/4095, đọc quá hạn hoặc mất mẫu.
- **GPIO:** nút active-low dùng pull-up và ngắt cạnh xuống để đánh thức task; D0 dùng ngắt hai cạnh để đánh thức sensor task; ISR chỉ notify task, không đọc cảm biến hay điều khiển relay. Relay active-high được đặt LOW lúc khởi tạo.
- **I2C master:** LCD qua PCF8574, 100 kHz. Có thể bật dò địa chỉ trong hai dải 0x20–0x27 và 0x38–0x3F; chế độ dò yêu cầu đúng một thiết bị phản hồi và không tự nhận diện mapping chân backpack.
- **UART0/serial monitor:** log ESP-IDF mặc định 115200 baud, dùng xem trạng thái startup, ADC, D0, Wi-Fi, relay và lỗi.
- **FreeRTOS:** task, queue giới hạn, semaphore đếm chỗ phản hồi, task notification và timer. Mốc thời gian dùng `esp_timer`.
- **NVS:** namespace `gas_cfg`, key `nguong_v1`; lưu ngưỡng có kiểm tra định dạng/giới hạn. NVS nằm trong flash và giữ dữ liệu sau reset.

### Web LAN: HTTP và WebSocket

ESP32 phục vụ `/`, `/app.js`, `/style.css` và WebSocket `/ws`; tài nguyên web được nhúng vào firmware. Wi-Fi dùng chế độ Station. Khi nhận IP, serial log URL `http://...`; trình duyệt cần ở cùng LAN. Web có tối đa 3 client và lệnh tối đa 384 byte.

- **Command JSON phiên bản 1:** `SET_MODE`, `SET_RELAY`, `BEGIN_EDIT`, `SET_DRAFT`, `SAVE_DRAFT`. Gateway từ chối field thiếu/thừa/trùng, kiểu hoặc giá trị sai; boot ID, control revision, edit session và draft revision ngăn lệnh cũ ghi đè trạng thái mới.
- **State snapshot phiên bản 2:** gửi khoảng mỗi 200 ms, gồm mode/sensor/relay/threshold và các trường lửa `fire_valid`, `fire`, `fire_confirming`. Snapshot dùng `seq` để bỏ dữ liệu cũ và `boot` để nhận biết ESP32 vừa reset.
- **Result:** `QUEUED` nghĩa là đã vào hàng đợi; `APPLIED` là lệnh đã xử lý; `PENDING` là đang lưu. Kết quả lưu cuối cùng đến qua snapshot. Browser không tự phát lại lệnh sau khi mất kết nối.
- Server kiểm tra `Origin` khớp `Host`, nhưng hiện không có đăng nhập hoặc TLS. Chỉ dùng trong mạng LAN tin cậy.

### Telegram Bot API qua HTTPS

Khi bật `GAS_TELEGRAM_ENABLED` và cấu hình Bot token/Chat ID, worker gửi JSON `sendMessage` bằng HTTPS tới `api.telegram.org`, xác minh chứng chỉ bằng certificate bundle và kiểm tra HTTP/API response. Đây là **kênh gửi cảnh báo đi**, không nhận lệnh Telegram.

Nếu worker đang bận, một sự kiện `FIRE_CLEARED` có thể được giữ lại để gửi sau request hiện tại; lửa mới xuất hiện sẽ hủy sự kiện hết lửa đang chờ. Các event bận khác, lỗi mạng/API hoặc cấu hình sai bị bỏ; không có retry tổng quát. Token được đưa vào firmware đã build và có thể bị trích xuất, vì vậy phù hợp demo hơn là nơi lưu bí mật yêu cầu bảo vệ cao.

## Kỹ thuật điều khiển và chính sách cảnh báo

- **Hysteresis gas:** ngưỡng hiệu lực mặc định 600 chỉ là giá trị bản nháp ban đầu; ngưỡng thực phải được thiết lập và lưu. Auto bật khi ADC `>= threshold`, tắt khi ADC `<= threshold - hysteresis`, giữ trạng thái giữa hai mức. Hysteresis mặc định 50 ADC count và có thể cấu hình 1–100; không phải đơn vị ppm.
- **Ưu tiên lửa trong Auto:** D0 hợp lệ phát hiện lửa yêu cầu relay ON bất kể MQ-2 đang warm-up hoặc gas config chưa sẵn sàng. Khi D0 hết lửa hợp lệ, Auto lập tức quay lại luật gas. D0 lỗi không được xem là phát hiện lửa hoặc xác nhận hết lửa; gas policy tiếp tục quyết định relay.
- **Telegram gas:** độc lập Auto/Manual và relay. Gas alarm cần cấu hình hợp lệ, cảm biến Ready và ADC đạt ngưỡng. Recovery yêu cầu 3 mẫu hợp lệ liên tiếp không vượt `threshold - hysteresis`; mẫu lỗi đặt lại đếm hồi phục.
- **Telegram lửa:** phát hiện theo D0 hợp lệ; thông báo hết lửa chỉ sau 3 giây liên tục có D0 hợp lệ báo không lửa. D0 báo lửa lại hoặc lỗi đọc sẽ hủy bộ đếm xác nhận. Thời gian 3 giây chỉ trì hoãn thông báo hết lửa, không trì hoãn quyết định relay.
- **Debounce:** mỗi nút chỉ phát event sau 30 ms ổn định; giữ nút không tự lặp. D0 được đọc lại sau khoảng chờ 30 ms khi ngắt đánh thức sensor task.
- **Lưu ngưỡng bất đồng bộ:** control task gửi job cho storage task; timeout 2 giây được hiển thị là kết quả chưa rõ và chờ kết quả giao dịch, không giả định Flash chắc chắn chưa ghi. Chỉnh sửa hết hạn sau 30 giây không thao tác.
- **Giới hạn queue và xử lý lỗi:** queue hữu hạn tránh dùng RAM không giới hạn; sample bị drop giữ sequence gap để core phát hiện mất dữ liệu. Snapshot LCD/web là mailbox một phần tử để luôn ưu tiên trạng thái mới nhất.

## Cấu hình và build/flash

`Gas_System/sdkconfig.defaults` cung cấp cấu hình ban đầu: ESP32, partition single-app lớn, MQ-2 warm-up 20 giây, hysteresis 50, D0 GPIO4 active-low, các GPIO như bảng trên, web bật, Telegram tắt, Wi-Fi trống. Giá trị đã lưu trong `sdkconfig` hoặc menuconfig cá nhân có thể khác. **Warm-up 20 giây chỉ là giá trị cấu hình mặc định của project, chưa phải thời gian đã xác nhận cho module MQ-2 của bạn.**

Mở terminal ESP-IDF 5.5.4 tại thư mục `Gas_System`:

```powershell
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Trong menuconfig, kiểm tra thời gian ổn định MQ-2, GPIO/cực tính D0, địa chỉ LCD, Wi-Fi và Telegram. Thay `COMx` bằng cổng thật. Để dùng web/Telegram, nhập Wi-Fi; Telegram cần thêm token và Chat ID. Không commit `sdkconfig` cá nhân chứa mật khẩu/token.

Sau boot, tìm log `su_kien=co_ip url=http://...` rồi mở URL từ thiết bị cùng mạng. Thoát monitor bằng `Ctrl+]`.

## Kiểm thử và giới hạn xác nhận

Một số file kiểm thử host độc lập còn nằm trong `components/*/test` và `components/app_tasks/button_task/test_button_debounce.c`; chúng không được đăng ký vào firmware component CMake và hiện không có một runner chung trong repository. `idf.py build` kiểm tra biên dịch/liên kết, không thay thế kiểm thử host hoặc thử nghiệm trên board.

Chưa xác nhận trên mạch thật: điện áp AO/D0, hiệu chuẩn hoặc ppm, thời gian warm-up phù hợp, nhiễu khi Wi-Fi/relay hoạt động, mapping backpack LCD, thời gian đáp ứng thực tế, tiếp điểm relay và khả năng gửi Telegram thực. UI web và formatter Telegram hiện gắn nhãn `ppm` cho giá trị ADC thô; hãy hiểu các con số đó là ADC count cho đến khi đổi nhãn/hiệu chuẩn. Relay là tải demo; project chưa được xác nhận là thiết bị báo cháy hoặc báo gas an toàn.

## Tài liệu liên quan

- [Sơ đồ nối dây](docs/wiring.md)
- [Cấu hình GPIO](docs/gpio-menuconfig.md)
- [Hướng dẫn Telegram](docs/telegram-guide.md)
- [Giao thức web và trạng thái](docs/protocol.md)
- [Bản đồ codebase](docs/codebase-map.md)
- [Core điều khiển](docs/code-walkthrough.md)
- [Task điều khiển và queue](docs/control-task-guide.md)
- [MQ-2 và sensor task](docs/sensor-guide.md)
- [Nút và relay](docs/button-task-guide.md)
- [LCD](docs/display-guide.md)
- [NVS](docs/storage-guide.md)
- [Wi-Fi và web](docs/web-guide.md)
