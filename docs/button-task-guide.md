# Đọc code nút nhấn và relay

Cập nhật 2026-09-25: đã có NVS và storage_task, SAVE thực hiện lưu thay cho
bộ phản hồi thất bại tạm thời. Xem [hướng dẫn lưu trữ](storage-guide.md).
Các ghi chú chưa có NVS ở bản hướng dẫn giai đoạn nút bên dưới là lịch sử.

## Phạm vi của lần triển khai này

Đã có driver relay, driver bốn nút, chống rung và `button_task` nối vào
`control_task`. Chưa có ADC MQ-2, LCD, Wi-Fi/web và NVS. Các giá trị thời gian,
ngưỡng và bước tăng 10 đang phục vụ thử luồng điều khiển, không phải ppm.

## Thứ tự đọc source (bỏ qua test)

1. `Gas_System/main/Gas_System.c`: điểm khởi động, chọn giả lập hoặc GPIO thật.
2. `Gas_System/components/drivers/relay/relay.c`: nạp LOW trước khi cấu hình
   GPIO23 output; HIGH bật, LOW tắt. Lỗi GPIO được trả về caller.
3. `Gas_System/components/drivers/buttons/buttons.c`: cấu hình GPIO25/26/27/32
   input pull-up; đổi mức LOW thành bit 1 tương ứng nút đang nhấn.
4. `Gas_System/components/app_tasks/button_task/nut_chong_rung.c`: bộ lọc thuần C,
   nhận thời gian và mặt nạ nút, trả về các cạnh nhấn đã ổn định.
5. `Gas_System/components/app_tasks/button_task/button_task.c`: đọc GPIO khoảng
   mỗi 10 ms, gọi bộ lọc rồi gửi cạnh nhấn vào queue bằng `dk_gui_nut()`.
6. `Gas_System/components/app_tasks/control_task/control_task.c`: tìm
   `dk_gui_nut()`, `dich_nut()`, `dk_chay_mot_luot()` rồi `xu_ly()`.
   Owner chuyển nút thành sự kiện lõi, kiểm tra điều kiện và gọi callback relay.

```text
GPIO nút → driver → chống rung → button_task → queue đầu vào
                                                  ↓
                                             control_task
                                                  ↓
                                             control_core
                                                  ↓
                                callback relay + snapshot LCD/web
```

Driver chỉ biết mức điện. `button_task` chỉ biết có cạnh nhấn. `control_task`
sở hữu trạng thái và quyết định ý nghĩa của nút tại thời điểm lấy khỏi queue.
Nút không đọc snapshot rồi tự sửa bản nháp: hai lần tăng liên tiếp vì vậy được
cộng lần lượt trên giá trị hiện hành, không ghi đè cùng một giá trị cũ.

## Chống rung và các trường hợp biên

Mỗi nút có mức ứng viên, thời điểm ứng viên đổi và mức đã ổn định. Chỉ công nhận
thay đổi nếu ứng viên không đổi ít nhất 30 ms. Khoảng 10/30 ms là thông số khởi
đầu để đo lại trên board; thời điểm thực tế còn phụ thuộc lịch chạy RTOS.
Giữ nút không tự lặp. Nút đang bị giữ khi task lấy mẫu đầu phải nhả ổn định rồi
nhấn lại. Bốn nút lọc độc lập; cạnh cùng lượt được gửi theo thứ tự ON, OFF,
MODE, EDIT. Đây là thứ tự xử lý xác định, chưa có chức năng tổ hợp phím.

Queue đầy: trả `ESP_ERR_TIMEOUT`, ghi log và bỏ cạnh đó; người dùng nhấn lại.
Không giữ cạnh để thử lại muộn vì lúc đó ngữ cảnh chỉnh ngưỡng có thể đã khác.
`ESP_OK` từ `dk_gui_nut()` chỉ xác nhận xếp hàng. Lõi vẫn có thể từ chối lệnh;
kết quả nằm trong log điều khiển và trạng thái mới trong snapshot.
Nút không chiếm queue phản hồi dành cho lệnh có mã định danh của gateway.

| Nút | Ngoài phiên chỉnh | Đang chỉnh | Đang lưu |
|---|---|---|---|
| GPIO25 ON/+ | Bật relay trong Manual | Tăng bản nháp 10 | Từ chối |
| GPIO26 OFF/- | Tắt relay trong Manual | Giảm bản nháp 10 | Từ chối |
| GPIO27 MODE | Đổi Auto/Manual | Từ chối | Từ chối |
| GPIO32 EDIT/SAVE | Mở bản nháp | Gửi yêu cầu lưu | Từ chối |

Giá trị tăng/giảm dừng ở biên cấu hình. Chỉnh ngưỡng không đổi relay thủ công.
Khi lưu thất bại, lõi giữ ngưỡng đang dùng và bản nháp. Khi hết thời gian không
thao tác, bản nháp bị hủy theo chính sách đã có trong lõi.

## Chạy thử trên board

Mặc định `CONFIG_DK_THU_NGHIEM=y`: chỉ giả lập callback relay, không cấu hình
GPIO nút/relay. Demo gửi hai lệnh Manual và ON; app_main tiêu thụ phản hồi.

Trong ESP-IDF PowerShell, vào `Gas_System`:

```powershell
idf.py -B build_control_task menuconfig
```

Trong `Thu nghiem dieu khien`, bỏ chọn `Chay du lieu gia, khong xuat GPIO`.
Sau đó build và flash bằng cổng thật của board:

```powershell
idf.py -B build_control_task build
idf.py -B build_control_task -p COMx flash monitor
```

Thay `COMx` bằng cổng của bạn. Dùng sơ đồ chân trong `docs/wiring.md`.
Ở chế độ GPIO, khởi động Auto/relay OFF. Chưa có cảm biến và ngưỡng đã lưu nên
Auto chưa bật được relay. Nhấn MODE sang Manual, rồi ON/OFF để kiểm tra GPIO23.
Nhấn EDIT rồi +/-: quan sát `su_kien` và kết quả trên serial; LCD chưa được viết.
Log `nguon=nut` có `nut`, `ket_qua`, `ban_nhap` và `nguong` để quan sát thay đổi.

Nhấn SAVE sẽ có log `su_kien=tu_choi_luu ly_do=chua_co_nvs`. app_main gửi
`DK_LUU_THAT_BAI` để giữ bản nháp; không giả vờ lưu thành công. Chờ 30 giây
không thao tác để hủy bản nháp rồi tiếp tục thử ON/OFF.

## Kiểm chứng và phần tiếp theo

Host tests dùng thời gian/GPIO/FreeRTOS giả để kiểm tra logic có tính lặp lại;
Đã đạt 123 kiểm tra lõi, 327 runtime, 20 chống rung và 28 driver GPIO.
Build ESP-IDF 5.5.4 thành công cho cả chế độ giả lập và chế độ GPIO.
không chứng minh thời gian thực, nhiễu điện hay mức chân vật lý. Chưa flash hoặc
đo board trong lần triển khai này. Kiểm tra vật lý cần ghi nhận mức GPIO23 lúc
reset, mỗi lần nhấn giữ/dội nút và thao tác trong/ngoài phiên chỉnh.

Sau khi thử nút/relay trên board, triển khai `storage_task` với NVS để lưu và
khôi phục ngưỡng. Sau đó nối MQ-2/ADC và `sensor_task`, LCD rồi web. Các producer
và giao diện đều dùng dịch vụ điều khiển hiện có.
