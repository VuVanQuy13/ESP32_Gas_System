# Interview - Gas_System

## Theo dõi bằng monitor

Chạy `idf.py -p COMx flash monitor` sau khi build; thay COMx bằng cổng board. Mức log mặc định hiện tại là INFO. Nếu đổi xuống WARN/ERROR, các dòng thông tin dưới đây sẽ không hiện.

- `nut: su_kien=nhan_on_dinh nut=BAT_TANG/TAT_GIAM/AUTO_MANUAL/CHINH_LUU`: cạnh nhấn đã qua chống rung. Đây là nhận nút, không khẳng định lệnh được chấp nhận. Xem thêm `nguon=nut ... ket_qua=...` ở control task.
- `cam_bien: su_kien=do_mau ... tho=... loc=... hop_le=... don_vi=ADC`: in mẫu đầu, khoảng mỗi giây và khi thay đổi tình trạng hợp lệ/gửi mẫu. `loc` chỉ có ý nghĩa khi `hop_le=1`; không phải ppm. Log không làm thay đổi chu kỳ lấy mẫu.
- `dieu_khien`: hiện AUTO/MANUAL, trạng thái cảm biến, relay yêu cầu, lỗi đầu ra, trạng thái chỉnh/lưu, ngưỡng và bản nháp. Mẫu bình thường không lặp log điều khiển mỗi lần đọc.
- `su_kien=canh_bao`: VUOT_NGUONG khi mẫu chạm/vượt ngưỡng, DUOI_NGUONG khi thấp hơn, CHUA_DU_DIEU_KIEN khi thiếu cấu hình hoặc cảm biến chưa sẵn sàng. Đây là so sánh ngưỡng, không thay thế hysteresis của relay; áp dụng cả Manual.
- `su_kien=xuat_relay`: callback xuất trả thành công; không xác nhận tiếp điểm relay vật lý. Chế độ giả không xuất GPIO thật.
- `nguon=web`: lệnh đã tới control task và mã kết quả. Mã kết quả nằm trong enum dk_ma_ket_qua ở control_core.h; 0 là thành công, 3 sai chế độ, 4 đang bận, 5 dữ liệu cũ, 7 lỗi lưu.
- `mang: su_kien=co_ip url=http://...`: địa chỉ truy cập sau khi có IP. Kết hợp `web: su_kien=http_san_sang cong=80 websocket=/ws` để biết HTTP đã khởi động; có IP riêng chưa chứng minh HTTP sẵn sàng.
- Có log thiếu/sai cấu hình Wi-Fi, mất kết nối kèm reason, kết nối WebSocket, ghi NVS, lỗi LCD và LCD phục hồi. Không in mật khẩu Wi-Fi hay payload web thô.

Các dòng log là sự kiện phần mềm; cần board thật để xác nhận nút, ADC, GPIO và mạng thực tế. Chế độ giả không chạy task nút cứng.

Mapping backpack LCD không còn nằm trong menuconfig. `main/Gas_System.c` gọi `lcd_so_do_mac_dinh()` trong `components/drivers/lcd1602/lcd1602.c`: P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7. Đây là đường nối nội bộ module, không phải GPIO ESP32. Khi thay backpack có mapping khác, cần sửa hàm này. Menu vẫn giữ GPIO SDA/SCL, địa chỉ/tự dò I2C và cực tính đèn nền.

`storage_task` hiện chỉ còn `storage_task.c/.h`. Header nội bộ đã được gộp: `luu_chay_mot_luot()` là hàm `static` trong `.c`, nhận việc lưu, ghi NVS rồi trả kết quả về control task. Nếu hàng đợi kết quả đầy, task giữ kết quả để gửi lại, không ghi NVS lần nữa. Header công khai giữ API `luu_bat_dau()`.

Tài liệu các component được gom ở đây. Các mục nguồn bên dưới giữ nội dung gốc, bao gồm tiến độ và kiểm thử lịch sử.

## Cách đọc các task sau khi gộp

`button_task.c`: vòng lặp đọc trạng thái bốn nút, gọi `nut_loc()` để chỉ nhận cạnh nhấn sau 30 ms ổn định, rồi gửi lệnh cho control task. Nó không tự quyết định relay.

`sensor_task.c`: `cb_bat_dau()` kiểm tra cấu hình và tạo task. Vòng lặp gọi `cb_chay_mot_luot()` để đọc ADC qua callback, kiểm tra thời gian và tính hợp lệ, gọi `cb_loc_mau()` lấy trung bình tối đa bốn mẫu, rồi gửi sự kiện có số thứ tự về control task. Đọc lỗi hoặc chạm biên ADC sẽ xóa lịch sử lọc. Không coi giá trị ADC là ppm đã hiệu chuẩn.

`sensor_task_noi_bo.h` trước đây chỉ khai báo hàm chạy một lượt cho nội bộ/kiểm thử; không tạo task riêng. Sau khi gộp, hàm đó là `static` trong `sensor_task.c`, chỉ dùng được trong file này. `sensor_task.h` vẫn là giao diện để main tạo task và truyền callback đọc ADC.

`display_task.c`: lấy bản chụp từ control task, định dạng hai dòng LCD, bỏ qua khung không đổi và gọi callback xuất. Driver `lcd1602_i2c.c` vẫn chịu trách nhiệm giao tiếp I2C.

`control_task_noi_bo.h` đã được gộp vào `control_task.c`: `dk_chay_mot_luot()` và `dk_so_tick_cho()` là hàm `static`, chỉ vòng lặp trong file này sử dụng. `control_task.h` giữ API công khai; module khác không gọi trực tiếp hai hàm nội bộ.

Driver LCD có hai tầng: `lcd1602.c/.h` tạo chuỗi lệnh/ký tự HD44780 qua PCF8574; `lcd1602_i2c.c/.h` dùng ESP-IDF để tạo bus, dò địa chỉ và truyền byte. Mỗi tầng có `.c` triển khai và `.h` khai báo giao diện.

Cau truc hien tai: button_task.c gom chong rung; sensor_task.c gom loc ADC; display_task.c gom dinh dang LCD. Cac helper va header noi bo tuong ung da duoc gop vao task. Giao tiep I2C van o driver LCD.

Cau hinh: idf.py menuconfig > Gas_System. Dinh nghia tap trung o main/Kconfig.projbuild. Wi-Fi nhap tai menu; wifi_local.h neu co duoc uu tien. Khong commit mat khau.

Build: idf.py build. Nap khi co board: idf.py -p COMx flash monitor. Mac dinh che do thu nghiem bat; MQ-2 on dinh bang 0 nghia la chua Ready.

Thu muc tools va tests hien rong; cac lenh runner cu ben duoi chi la tai lieu lich su. Web duoc CMake nhung vao firmware.

---

### Tai lieu goc: `components/app_tasks/button_task/README.md`

# button_task

Read the buttons driver, debounce inputs and emit button events. Mode-dependent button meanings belong to control logic. Do not write shared state or command the relay.

Đã triển khai `button_task.c/h` và bộ lọc thuần C `nut_chong_rung.c/h`. Đọc khoảng 10 ms/lần, ổn định 30 ms mới nhận cạnh, không lặp khi giữ, bỏ nút bị giữ lúc khởi động đến khi nhả. Gửi `dk_gui_nut`, queue đầy thì ghi log và bỏ cạnh. Xem `docs/button-task-guide.md` ở thư mục gốc dự án.


---

### Tai lieu goc: `components/app_tasks/control_task/README.md`

# control_task

Sở hữu trạng thái chính thức, nhận sự kiện, gọi control_core và chuyển kết quả sang callback relay, hai mailbox LCD/web và queue lưu. Nhận kết quả lưu qua kênh riêng. Không chờ consumer LCD/web.

Đã có control_task.c/.h; header nội bộ chỉ cho vòng lặp tác vụ và test đơn luồng. Không gọi dk_chay_mot_luot từ task khác. Test mô phỏng đạt 243 checks; chưa kiểm chứng lịch chạy FreeRTOS trên board. Hướng dẫn: docs/control-task-guide.md.


---

### Tai lieu goc: `components/app_tasks/display_task/README.md`

# display_task

Đã có formatter thuần C và task đọc snapshot LCD, hiển thị trạng thái/mẫu/
ngưỡng/bản nháp/lỗi. Chỉ ghi khi nội dung đổi; lỗi thì giãn retry và dùng
snapshot mới nhất. Không thay trạng thái control. Xem docs/display-guide.md.

Đã kiểm thử với fake I2C/FreeRTOS. Bản demo xuất hai dòng qua serial.


---

### Tai lieu goc: `components/app_tasks/README.md`

# app_tasks

Các thư mục chứa tác vụ ứng dụng. Đã triển khai control_task và đăng ký component CMake; năm tác vụ còn lại mới có mô tả trách nhiệm. Logic nghiệp vụ nằm trong control_core; tác vụ điều phối sự kiện và I/O.

control_task có 243 phép kiểm tra mô phỏng API FreeRTOS trên máy. Chưa build/flash thành công trên ESP-IDF. Xem docs/control-task-guide.md.


---

### Tai lieu goc: `components/app_tasks/sensor_task/README.md`

# sensor_task

Đã triển khai `sensor_task.c/h` và `cam_bien_loc.c/h`: đọc callback ADC,
lọc trung bình bốn mẫu, gửi sequence/timestamp/chất lượng vào control_task.
Lỗi, chạm biên hoặc gián đoạn làm reset bộ lọc. Queue đầy thì bỏ mẫu, không
gửi lại dữ liệu cũ. Task không trực tiếp điều khiển relay.

Đã kiểm thử source thật bằng fake ADC/FreeRTOS. Thời gian ổn định MQ-2 thật
chưa xác định: mặc định khóa Ready qua cấu hình startup. Xem `docs/sensor-guide.md`.


---

### Tai lieu goc: `components/app_tasks/storage_task/README.md`

# storage_task

Đã triển khai: nhận việc qua `dk_nhan_viec_luu`, gọi component `storage`, gửi
kết quả đúng mã boot/lần lưu qua `dk_bao_ket_qua_luu`. Queue đầy thì giữ kết quả
và gửi lại, không ghi NVS lại. Xem `docs/storage-guide.md` ở gốc workspace.
Task không đổi ngưỡng hiệu lực trực tiếp. Chỉ control_task cập nhật trạng thái.
Đã có kiểm thử tích hợp với NVS/FreeRTOS giả và build ESP-IDF; chưa thử board.


---

### Tai lieu goc: `components/drivers/buttons/README.md`

# buttons

Read four button GPIO levels with verified polarity/pull configuration. button_task performs debounce and emits button events; control logic assigns application meaning.

Đã triển khai `buttons.c/h`: GPIO25/26/27/32 input pull-up, LOW là nhấn. Trả mặt nạ bốn bit, không chống rung hoặc sửa trạng thái ứng dụng. Đã kiểm thử GPIO giả và build ESP-IDF; chưa đo trên board.


---

### Tai lieu goc: `components/drivers/lcd1602/README.md`

# lcd1602

Latest user correction: LCD1602 uses a PCF8574 I2C backpack. The earlier Waveshare PDF does not apply. Verify I2C address, backpack pin mapping, supply and pull-up levels during hardware bring-up. No RGB control is specified. See docs/hardware.md.

Đã có lcd1602.c/h (HD44780 4-bit qua callback byte PCF8574) và lcd1602_i2c.c/h
(ESP-IDF I2C master, GPIO21/22). Sơ đồ chân, cực tính đèn nền và địa chỉ cấu
hình được. Đã kiểm thử giao thức/I2C giả; chưa kiểm tra LCD thật.
Xem docs/display-guide.md ở gốc workspace.


---

### Tai lieu goc: `components/drivers/mq2/README.md`

# mq2

Đã triển khai `mq2.c/h`: ESP-IDF oneshot ADC1 channel 6/GPIO34, 12 bit,
attenuation 12 dB. AO qua chia áp, không dùng DO. Trả mã thô/lỗi và chỉ sửa
đầu ra khi đọc thành công. Không chứa lọc hoặc quyết định relay.

Đã kiểm thử với ADC giả; chưa đo điện áp hoặc hiệu chỉnh trên board.
Xem `docs/sensor-guide.md` ở gốc workspace. Chưa chuyển mV hoặc ppm.


---

### Tai lieu goc: `components/drivers/README.md`

# drivers

Các driver giao tiếp phần cứng được tách theo module: mq2, lcd1602, buttons, relay. Driver không chứa chính sách Auto/Manual, chỉnh ngưỡng hoặc giao thức web.

`Kconfig.projbuild` cung cấp menu `Chan GPIO - Gas System (ESP32 DevKit V1)`.
`board_pins.h` kiểm tra chân hợp lệ/trùng ngay khi build và ánh xạ chân MQ-2 sang kênh ADC1.
Các driver dùng `CONFIG_GAS_GPIO_*`, không cố định số chân trong source.
Xem [hướng dẫn đổi chân](../../../docs/gpio-menuconfig.md).

Đã đăng ký component ESP-IDF qua CMakeLists.txt. Relay, buttons, MQ-2 và LCD
đã có source/header và kiểm thử transport giả. LCD dùng esp_driver_i2c;
MQ-2 dùng esp_adc; nút/relay dùng esp_driver_gpio. Chưa thử phần cứng thực tế.


---

### Tai lieu goc: `components/drivers/relay/README.md`

# relay

User-confirmed polarity: active-high, ON = HIGH and OFF = LOW. Initialize GPIO to OFF. No Auto/Manual policy. User explicitly limits this driver scope to GPIO output; do not block on further relay module investigation. No physical relay verification is claimed. The LED load uses 3.3 V.

Đã triển khai `relay.c/h`: GPIO23 active-high, khởi tạo LOW trước output, trả lỗi cho caller. Đã kiểm thử GPIO giả và build ESP-IDF; chưa đo trên board. Xem `docs/button-task-guide.md` ở thư mục gốc dự án.


---

### Tai lieu goc: `components/network/README.md`

# Mạng Wi-Fi

Profile bàn giao dùng `config/board.local.json` và `tools/firmware.ps1`; CMake ưu tiên header sinh trong build qua GAS_WIFI_CONFIG_DIR. Xem [hướng dẫn nạp](../../../docs/flash-guide.md). Cách đặt header trực tiếp bên dưới vẫn hỗ trợ cho build thủ công.

`network.c` khởi tạo STA và thử nối lại khi mất kết nối. Sao chép `include/wifi_local.example.h` thành file cá nhân `include/wifi_local.h` để cấu hình mạng; file đích được gitignore.

Không có cấu hình thì trả `ESP_ERR_NOT_FOUND`; phần điều khiển cục bộ tiếp tục chạy. Lỗi khởi tạo được trả về và ghi log, cần reset sau khi sửa nguyên nhân; chưa hỗ trợ khởi tạo lại toàn bộ mạng ngay trong cùng lần chạy.

Hướng dẫn: [web-guide.md](../../../docs/web-guide.md).


---

### Tai lieu goc: `components/storage/README.md`

# Lưu trữ cấu hình

Component NVS độc lập với RTOS và state machine. API: `luu_khoi_tao`, `luu_doc`,
`luu_ghi`. Một bản ghi u64 có nhận dạng, phiên bản và ngưỡng; kiểm tra giới hạn
khi đọc/ghi; commit trước khi báo thành công. Không tự erase khi lỗi NVS.
Khởi động đọc một lần, sau đó chỉ storage_task truy cập. Xem
`docs/storage-guide.md` ở gốc workspace để đọc luồng và giới hạn kiểm thử.


---

### Tai lieu goc: `components/web_service/README.md`

# Dịch vụ web LAN

- `web_gateway.c`: kiểm tra giao thức, chống lặp lệnh, chuyển lệnh vào queue và tạo snapshot JSON.
- `web_task/web_task.c`: HTTP/WebSocket và task lên lịch phát snapshot.
- `web/`: giao diện tiếng Việt, không phụ thuộc CDN.

HTTP server là chủ sở hữu gateway và consumer duy nhất của queue phản hồi lệnh web. `control_task` vẫn sở hữu trạng thái hệ thống. Không gọi gateway đồng thời từ task khác.

Hướng dẫn: [web-guide.md](../../../docs/web-guide.md). Giao thức: [protocol.md](../../../docs/protocol.md).

