# MQ-2/ADC và sensor_task

Cập nhật giai đoạn LCD: đã có display_task đọc snapshot và LCD1602/PCF8574.
Demo in hai dòng LCD qua serial; xem [hướng dẫn hiển thị](display-guide.md).

## Mục đích và phạm vi

Đã có đường lấy mẫu từ ADC đến lõi điều khiển. Chưa có board: kiểm thử source
thật với ADC và FreeRTOS giả trên PC. Kết quả đo dùng đơn vị ADC thô, chưa có
chuyển đổi mV, điện trở Rs/R0 hoặc ppm. Ngưỡng NVS hiện được hiểu theo cùng đơn
vị ADC thô; thay đơn vị/attenuation/mạch chia áp cần xem lại định dạng lưu và ngưỡng.

## Thứ tự đọc source (bỏ qua test)

1. `Gas_System/components/drivers/mq2/mq2.h`: kiểu `mq2_kenh` và hai API.
2. `Gas_System/components/drivers/mq2/mq2.c`: tạo ADC, cấu hình kênh và đọc mẫu.
3. `Gas_System/components/app_tasks/sensor_task/cam_bien_loc.c`: lọc thuần C.
4. `Gas_System/components/app_tasks/sensor_task/sensor_task.h`: đầu vào cấu hình,
   callback đọc ADC và vòng đời task.
5. `Gas_System/components/app_tasks/sensor_task/sensor_task.c`: lấy mẫu, kiểm tra
   thời gian, gửi sự kiện; xem `cb_chay_mot_luot()` trước rồi đọc vòng lặp task.
6. `Gas_System/main/Gas_System.c`: chọn nguồn ADC thật hoặc giả, nối dịch vụ.

```text
MQ-2 AO → chia áp → GPIO34 → mq2_doc()
                                  ↓
                             sensor_task
                        thời gian + lọc + sequence
                                  ↓
                         queue DK_MAU_MOI
                                  ↓
                             control_task
                                  ↓
                     control_core quyết định Ready/FAULT
                         rồi quyết định relay Auto
```

## Driver chỉ phụ trách giao tiếp ADC

`mq2_khoi_tao()` cấu hình ADC1, channel 6 (GPIO34), 12 bit, attenuation 12 dB.
ADC2 có chia sẻ tài nguyên với Wi-Fi, vì vậy dùng ADC1 theo sơ đồ chân đã chọn.
API oneshot trả mã thô; lỗi timeout không tạo mẫu hợp lệ. Tham khảo
[hướng dẫn ADC oneshot ESP-IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/adc_oneshot.html).

Nếu cấu hình kênh thất bại, driver cố thu hồi ADC vừa cấp. Nếu thu hồi cũng lỗi,
driver giữ handle, ghi log và không cho đọc hoặc khởi tạo chồng lên tài nguyên.
Lỗi khởi tạo được app_main ghi log; task vẫn gửi mẫu lỗi, Manual vẫn sử dụng được.
Chưa có cơ chế tự tái khởi tạo ADC; cần reset sau khi khắc phục lỗi khởi tạo.

`mq2_doc()` đọc vào biến tạm, chỉ gán đầu ra khi thành công và mã thuộc 0..4095.
Đây là kết quả giao tiếp hợp lệ; kiểm tra chất lượng tín hiệu nằm ở tầng sau.
Driver không chứa ngưỡng, Auto/Manual hoặc lệnh bật relay. AO phải đi qua mạch
chia áp theo `docs/wiring.md`; cấu hình attenuation không thay cho chia áp.

## Bộ lọc trung bình bốn mẫu

`cb_bo_loc` có mảng vòng bốn phần tử, tổng hiện tại, vị trí ghi và số mẫu.
Khi đủ bốn mẫu, trừ mẫu cũ khỏi tổng trước khi thêm mẫu mới. Giá trị đầu ra
là tổng chia số mẫu hiện có, làm tròn xuống. Ví dụ:

| Các mẫu gần nhất | Đầu ra |
|---|---:|
| 100 | 100 |
| 100, 200 | 150 |
| 100, 200, 300, 400 | 250 |
| 200, 300, 400, 500 | 350 |

Không chờ đầy cửa sổ mới xuất mẫu; lõi vẫn kiểm tra riêng chuỗi mẫu liên tiếp
và thời gian ổn định. Với khoảng đọc 100 ms, cửa sổ chứa tối đa khoảng 300 ms
lịch sử trong điều kiện chạy đều. Trung bình làm trễ đáp ứng khi khí thay đổi;
cửa sổ này là lựa chọn thử nghiệm cần đánh giá trên dữ liệu thực.

Lỗi đọc, giá trị ngoài miền hoặc chạm biên 0/4095 sẽ xóa lịch sử lọc và trả
`hop_le=false`. Chính sách loại biên là kiểm tra chất lượng thử nghiệm, không
phải bằng chứng chẩn đoán hỏng cảm biến. Bão hòa ở mức cao cũng thành FAULT;
với tải LED demo, Auto tắt theo yêu cầu đã chốt, không được diễn giải là khí an toàn.
Một chân AO trôi ở giữa miền vẫn có thể vượt kiểm tra này. Chưa có chẩn đoán
đứt dây, kiểm tra heater hoặc mô hình đáp ứng MQ-2.

## Một lượt sensor_task làm gì?

1. Lấy thời điểm trước khi gọi ADC. Từ lượt thứ hai, thời gian phải tăng.
2. Đọc ADC qua callback. Trong firmware thật callback gọi `mq2_doc`; demo trả
   dữ liệu giả. Task không cần biết nguồn dữ liệu thuộc loại nào.
3. Nếu khoảng cách từ lần đọc trước hoặc thời gian thực hiện đọc đạt 500 ms,
   xóa bộ lọc và gửi mẫu không hợp lệ. Không giữ lịch sử qua một gián đoạn.
4. Lọc mẫu và tạo `DK_MAU_MOI`: boot ID, sequence, timestamp, chất lượng và giá trị.
5. Gửi qua `dk_gui_mau()`. Queue đầy thì bỏ mẫu và reset bộ lọc; không gửi lại
   mẫu cũ. Sequence vẫn tăng nên lõi nhận ra thiếu mẫu ở lần nhận tiếp theo.
6. Vòng lặp chờ khoảng 100 ms sau khi làm việc, không đọc dồn để bù chu kỳ lỡ.

Timestamp là thời điểm trước lời gọi đọc, giúp không làm mới giả một lần đọc
bị kéo dài. Nếu mẫu nằm trong queue quá lâu, lõi kiểm tra tuổi mẫu một lần nữa.
`ESP_OK` từ `cb_chay_mot_luot()` chỉ nghĩa sự kiện đã vào queue; đọc ADC lỗi
vẫn có thể xếp hàng thành công với `hop_le=false`.

Task sở hữu sequence/bộ lọc; chỉ control_task sở hữu trạng thái điều khiển.
Stack khởi điểm 3072 byte, priority 4. Chu kỳ thực tế phụ thuộc scheduler và
thời gian đọc/log. Các thông số này chưa được đo trên board.

## Sẵn sàng, lỗi và phục hồi

Lõi hiện kiểm tra đủ thời gian ổn định, chuỗi ba mẫu hợp lệ liên tiếp và dữ
liệu còn mới. Lỗi đọc hoặc quá hạn làm Auto tắt relay và báo FAULT. Sau lỗi,
cần đủ chuỗi mẫu hợp lệ để trở lại Ready. Manual vẫn theo nút/web khi lỗi,
nhưng trạng thái lỗi cảm biến không bị xóa bởi việc bật relay.

`CONFIG_MQ2_ON_DINH_GIAY` mặc định 0, nghĩa là thời gian ổn định MQ-2 thật chưa
được xác định. Trong chế độ phần cứng, app_main dùng `UINT64_MAX` cho thời gian
ổn định để chưa cho Ready. Vẫn đọc, lọc và quan sát log được; lỗi ADC vẫn báo
FAULT. Khi có thời gian được xác định qua tài liệu/thực nghiệm, đặt giá trị
khác 0 trong menuconfig. Đây không thay thế quá trình già hóa và hiệu chỉnh.

Chu kỳ 100 ms, hạn dữ liệu 500 ms, ba mẫu liên tiếp và ngưỡng/hysteresis hiện
vẫn phục vụ thử nghiệm. Việc đặt thời gian ổn định khác 0 không tự xác nhận
toàn bộ cấu hình cảm biến đã được hiệu chỉnh.

## Demo không cần giao tiếp phần cứng

`CONFIG_DK_THU_NGHIEM=y` không khởi tạo ADC, nút hoặc relay thật. NVS vẫn là
NVS thật nếu chạy firmware trên ESP32. Callback mẫu giả lặp mỗi 15 giây:
400 trong 5 giây, 900 trong 5 giây, lỗi timeout trong 1 giây, rồi 400 trong 4 giây.
Mốc chu kỳ dựa trên thời gian từ boot. Demo dùng thời gian ổn định 1 giây chỉ
cho dữ liệu giả; không áp dụng cho MQ-2 thật.

Khởi động hiện giữ Auto/OFF, đã bỏ hai lệnh demo tự chuyển Manual/ON trước đây.
Nếu chưa có ngưỡng hợp lệ trong NVS, relay demo vẫn OFF; chương trình không tự
lưu hoặc giả vờ có cấu hình. Host integration test cung cấp ngưỡng giả 600 để
kiểm tra đầy đủ nhánh relay bật/tắt và phục hồi.

Log `cam_bien` có `tho`, `loc`, `hop_le`, `loi_adc`, `dut_mau`, `qua_han`,
`loi_gui`. Khi `hop_le=0`, không được đọc `loc=0` thành nồng độ bằng 0.
Snapshot lõi giữ số đo hợp lệ trước đó và trạng thái lỗi riêng.

## Chạy kiểm thử trên PC

Kết quả: 234 phép kiểm tra cảm biến đạt, tổng toàn bộ suite 868 phép.
Build ESP-IDF 5.5.4 thành công ở cả chế độ demo và ADC/GPIO thật.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/sensor/run.ps1
```

Source thật được biên dịch với fake ADC và fake FreeRTOS. Log trong
`Gas_System/tests/sensor/out/build.log` và `test.log`.
Các kiểm tra gồm cấu hình ADC/thu hồi khi lỗi, giữ đầu ra khi timeout, lọc vòng,
reset lịch sử, startup, Auto/Manual, mẫu biên, queue đầy, timeout, đọc chậm,
phục hồi và khóa Ready khi thời gian ổn định chưa xác định.

Host tests không kiểm chứng độ chính xác ADC, điện áp, nhiễu, lịch chạy SMP,
ảnh hưởng ghi NVS lên lấy mẫu, thời gian ổn định hoặc nồng độ khí thực tế.
Phần tiếp theo là LCD1602/PCF8574 và display_task đọc snapshot.
