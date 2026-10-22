# LCD1602/PCF8574 và display_task

## Thứ tự đọc source

1. `Gas_System/components/drivers/lcd1602/lcd1602.h`: khung hai dòng, sơ đồ
   nối chân backpack và callback ghi/chờ.
2. `lcd1602.c` cùng thư mục: tách byte thành hai nibble, phát xung E, khởi tạo
   chế độ 4-bit và ghi DDRAM hai dòng.
3. `lcd1602_i2c.c`: nối callback với ESP-IDF I2C master, SDA21/SCL22, 100 kHz.
4. `Gas_System/components/app_tasks/display_task/hien_thi_khung.c`: chuyển
   snapshot thành hai dòng, không gọi phần cứng hoặc thay trạng thái lõi.
5. `display_task.c`: đọc snapshot, so sánh nội dung, xuất và xử lý retry.
6. `Gas_System/main/Gas_System.c`: chọn LCD giả qua serial hoặc LCD I2C thật.

```text
control_task → mailbox LCD → display_task → tạo khung 2×16
                                               ↓
                                    driver LCD → I2C → PCF8574
```

Mailbox LCD và web độc lập. Display chỉ đọc bản sao, không lấy mất snapshot
của web và không gửi lệnh thay ngưỡng hoặc relay.

## Nội dung hiển thị

Ví dụ Manual, relay được yêu cầu bật, mẫu vượt ngưỡng, đang chỉnh bản nháp:

```text
M R:1 VUOT  800 
CHINH: 700  F:1
```

Source luôn tạo đúng 16 ký tự mỗi dòng, thêm khoảng trắng để xóa phần chữ cũ.
Ký tự dùng ASCII; tiếng Việt có dấu cần thiết kế bộ ký tự riêng, chưa triển khai.

| Nội dung | Ý nghĩa |
|---|---|
| `A` / `M` | Auto / Manual |
| `R:0` / `R:1` | Trạng thái đầu ra do phần mềm yêu cầu; chưa có cảm biến phản hồi tiếp điểm |
| `R:?` | Có lỗi xuất GPIO hoặc snapshot quá cũ, không khẳng định đầu ra |
| `ADC: ...` | Mẫu đã lọc, hợp lệ và còn mới |
| `VUOT ...` | Mẫu >= ngưỡng đang dùng, kể cả Manual |
| `ON DINH` | Đang chờ ổn định, chưa Ready; không có nghĩa khí an toàn |
| `LOI CB` | Lỗi cảm biến hoặc mẫu quá hạn; không hiển thị mẫu cũ như số đo mới |
| `MAT DONGBO` | Snapshot không cập nhật ít nhất 1 giây hoặc thời gian không hợp lệ |
| `CAN DAT NGUONG` | Chưa có ngưỡng hiệu lực hợp lệ |
| `CHINH: ...` | Bản nháp dùng chung giữa nút và web tương lai |
| `DANG LUU: ...` | Đang chờ kết quả lưu |
| `LUU CHUA RO` | Lưu quá hạn nhưng chưa biết kết quả cuối |
| `LOI LUU: ...` | Lưu thất bại; giữ bản nháp cho lần thử lại |
| `F:1` | D0 đọc hợp lệ và đang phát hiện lửa |
| `F:0` | D0 đọc hợp lệ và không phát hiện lửa |
| `F:?` | Chưa có lần đọc D0 hợp lệ hoặc lần đọc D0 gần nhất bị lỗi |

Lỗi cảm biến nằm ở dòng đầu, nên vẫn nhìn thấy trong lúc chỉnh/lưu ở dòng hai.
Khi snapshot quá cũ, dòng hai chuyển sang `CHO DU LIEU MOI`.
`R:?` không thể thay cho đo mức chân hoặc tiếp điểm relay trên board.

Ba ô cuối dòng hai luôn dành cho `F:1`, `F:0` hoặc `F:?`. Nội dung chính của
dòng hai chỉ dùng tối đa 13 ô để không ghi đè trạng thái lửa.

Snapshot được bổ sung cờ `loi_luu`, do control_task sở hữu: giữ sau lỗi Save,
không mất khi có mẫu mới. Cờ được xóa khi bắt đầu phiên chỉnh mới, thử lưu lại
hoặc lưu thành công. LCD chỉ đọc cờ này; không tự suy luận lỗi từ thời gian.

## Driver LCD hoạt động thế nào?

PCF8574 nhận một byte để xuất tám chân song song. Sơ đồ phổ biến mặc định:
P0=RS, P1=RW, P2=E, P3=đèn nền, P4..P7=D4..D7. Đây là giả định cấu hình,
không xác nhận từ ảnh. RW luôn LOW, chỉ ghi; dùng thời gian chờ thay busy flag.

Mỗi nibble: đặt data khi E thấp → E cao → E thấp. Mỗi byte gửi nibble cao
trước, nibble thấp sau. Driver giữ delay cho setup/xung E và lệnh LCD.
Khởi tạo gồm chờ nguồn, chuỗi 3/3/3/2, function set, display off, clear,
entry mode và display on. Dòng 1 bắt đầu tại DDRAM 0x00, dòng 2 tại 0x40.
Clear chỉ dùng lúc khởi tạo, không gọi cho mỗi lần cập nhật.

Tham khảo [datasheet HD44780U](https://www.sparkfun.com/datasheets/LCD/HD44780.pdf),
[datasheet PCF8574](https://www.ti.com/lit/ds/symlink/pcf8574.pdf) và
[I2C master ESP-IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/i2c.html).
Chưa xác nhận controller tương thích và thời gian trên LCD thực tế.

## Cấu hình phần cứng dự kiến

Trong menu `Thu nghiem dieu khien`:

- `LCD_DIA_CHI`: mặc định địa chỉ 7-bit 0x27. Chấp nhận dải PCF8574 0x20..0x27
  hoặc PCF8574A 0x38..0x3F; không tự quét hoặc đoán địa chỉ.
- `LCD_SO_DO`: mặc định 0x76543210, tám chữ số hex lần lượt là vị trí chân
  D7 D6 D5 D4 BL E RW RS. Mỗi chữ số phải thuộc 0..7 và không trùng nhau.
- `LCD_DEN_MUC_CAO`: bật đèn nền ở mức HIGH; có thể đổi cho backpack khác.

SDA21/SCL22 dùng pull-up ngoài theo `docs/wiring.md`; internal pull-up bị tắt.
Nếu backpack cấp 5 V, giữ mạch chuyển mức đã đề xuất. Không dùng lại datasheet
Waveshare RGB mà người dùng đã đính chính. Điện áp, địa chỉ và sơ đồ chân đều
phải xác nhận khi có board.

## Lỗi I2C và cập nhật

Task kiểm tra snapshot khoảng mỗi 200 ms, priority 2, stack khởi điểm 3072 byte.
Chỉ ghi khi khung đổi hoặc cần thử lại lần ghi lỗi. Một lần ghi xuất đủ hai
dòng; việc cập nhật vật lý diễn ra tuần tự, không phải thay cả màn hình tức thì.

Mỗi giao dịch byte I2C có timeout 20 ms. Lỗi đầu tiên dừng lần ghi, đánh dấu LCD
chưa sẵn sàng. Display task ghi log, chờ ít nhất 1 giây tính từ lần thử trước
rồi lấy snapshot mới. Driver khởi tạo lại LCD trước khi ghi để phục hồi lệch
nibble do lần truyền dang dở. Không cập nhật cache nội dung khi ghi thất bại.

Bus/device được giữ đến reset. Nếu tạo bus thành công nhưng add-device lỗi,
lượt sau chỉ thử add-device; không cấp bus chồng lên bus cũ. Chỉ display_task
truy cập driver trong runtime. Lỗi LCD không đổi ngưỡng, chế độ hoặc relay.

Không có polling phát hiện LCD mất nguồn khi nội dung đứng yên. Nếu thiết bị
reset rồi quay lại giữa hai lần ghi mà không có NACK/timeout được quan sát,
phần mềm có thể chưa biết để khởi tạo lại; cần kiểm thử thêm khi có board.
Thông báo trên LCD cũng không đọc được khi chính màn hình mất kết nối: khi đó
dùng log serial để xem lỗi. Chưa có web đưa lỗi LCD ra giao diện.

## Chạy không cần board

Đã đạt 1948 phép kiểm tra hiển thị (gồm kiểm tra từng byte truyền), tổng các
bộ kiểm thử 2820 phép. Build ESP-IDF 5.5.4 đã đạt cả demo và phần cứng.
Chưa flash hoặc xác nhận trên LCD thực tế.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/display/run.ps1
```

Runner biên dịch driver giao thức, adapter I2C, formatter, display_task và
control_task thật với fake I2C/FreeRTOS. Kiểm tra chuỗi byte/timing tối thiểu,
DDRAM, nội dung 16 ký tự, lỗi/thu hồi trạng thái, retry và snapshot mới nhất.
Log trong `Gas_System/tests/display/out/`. Fake không xác nhận mức điện, timing
thực tế, scheduler SMP, độ tương phản, ký tự ROM hay địa chỉ backpack.

Ở firmware demo mặc định, callback `xuat_lcd_gia` in `dong_1` và `dong_2` ra
serial, không cấu hình I2C. NVS vẫn dùng thật nếu firmware chạy trên ESP32.
Phần tiếp theo là Wi-Fi/web trong LAN và đồng bộ lệnh/snapshot.
