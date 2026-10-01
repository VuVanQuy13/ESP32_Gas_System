# Module lưu ngưỡng và storage_task

Cập nhật giai đoạn cảm biến: đã có ADC/sensor_task, demo dùng nguồn mẫu giả
và giữ Auto/OFF khi khởi động thay cho hai lệnh Manual/ON cũ. Xem
[hướng dẫn cảm biến](sensor-guide.md); mô tả demo cũ bên dưới là lịch sử.

## Phạm vi đã triển khai

Module này lưu ngưỡng bằng NVS của ESP-IDF và nối vào state machine hiện có.
Chưa có board: kiểm thử chạy source thật với API NVS/FreeRTOS giả trên PC.
Thông số ngưỡng vẫn là đơn vị thử nghiệm, không phải ppm đã hiệu chỉnh.

## Cấu trúc và thứ tự đọc source

1. `Gas_System/components/storage/include/storage.h`: hợp đồng đọc/ghi.
2. `Gas_System/components/storage/storage.c`: thao tác NVS và kiểm tra bản ghi.
3. `Gas_System/components/app_tasks/storage_task/storage_task.h`: khởi động task.
4. `Gas_System/components/app_tasks/storage_task/storage_task.c`: lấy việc, ghi,
   giữ kết quả và gửi về control_task.
5. `Gas_System/main/Gas_System.c`: đọc ngưỡng trước khi chạy dịch vụ điều khiển.
6. `Gas_System/components/control_core/control_core.c`: xem các nhánh
   `DK_YEU_CAU_LUU`, `DK_LUU_THANH_CONG`, `DK_LUU_THAT_BAI` đã có từ trước.

`storage` là component lưu trữ riêng, không đặt chung với driver cảm biến.
Nó phụ thuộc NVS, không biết Auto/Manual hoặc queue. `storage_task` nối component
này với API điều khiển; không được sửa trực tiếp trạng thái lõi.

## 1. Khởi động và đọc lại ngưỡng

Trong `app_main`, cấu hình GPIO thật đưa relay về OFF trước khi khởi tạo NVS.
`luu_khoi_tao()` nhận giới hạn ngưỡng và gọi `nvs_flash_init()` đúng một lần cho
kho. `luu_doc()` mở namespace `gas_cfg` ở chế độ chỉ đọc và đọc key `nguong_v1`.
Chưa có namespace/key thì không tạo ngưỡng mặc định rồi coi là đã lưu.

Nếu đọc và kiểm tra thành công, `du_lieu_luu_hop_le=true` và `nguong_da_luu`
được truyền vào `dk_bat_dau()`. Nếu lỗi, ứng dụng ghi log `can_cau_hinh` và
khởi động lõi với cấu hình chưa hợp lệ. Manual vẫn dùng được theo yêu cầu;
Auto không điều khiển relay khi chưa đủ điều kiện.

Không có nhánh tự động erase NVS. Lỗi khởi tạo được giữ trong `luu_kho`, các
yêu cầu lưu sau đó trả lỗi tương ứng cho đến reset/khắc phục nguyên nhân.
Lỗi đọc bản ghi không làm vô hiệu khả năng ghi mới nếu NVS đã khởi tạo được.

## 2. Nội dung bản ghi

Một key kiểu `uint64_t` chứa:

| Vị trí | Nội dung |
|---|---|
| Bit 63..48 | Mã nhận dạng `0x4D51` |
| Bit 47..32 | Phiên bản định dạng `1` |
| Bit 31..0 | Ngưỡng không âm |

Dùng một giá trị số giúp tránh lưu struct có padding và tránh tách phiên bản,
ngưỡng thành nhiều key. `luu_doc()` kiểm tra phần nhận dạng/phiên bản và giới
hạn trước khi gán đầu ra. Nếu lỗi, biến đầu ra của caller không bị thay đổi.
Chưa hỗ trợ migration định dạng hoặc thay đơn vị đo. Khi đổi đơn vị/ý nghĩa
ngưỡng, phải đổi định dạng hoặc viết migration trước khi tái sử dụng dữ liệu.

## 3. Khi nhấn SAVE

```text
Nút/web → control_task → control_core tạo yêu cầu lưu
                             ↓
                        queue viec_luu
                             ↓
                        storage_task
                             ↓
                 luu_ghi → set_u64 → commit → close
                             ↓
                       queue ket_qua_luu
                             ↓
                        control_task
                             ↓
                  control_core → snapshot giao diện
```

`luu_chay_mot_luot()` lấy tối đa một yêu cầu. Hàm gọi `luu_ghi()` với ngưỡng
trong yêu cầu, rồi tạo sự kiện kết quả mang nguyên `ma_khoi_dong` và
`ma_lan_luu`. `luu_ghi()` chỉ trả `ESP_OK` sau set và commit đều thành công.
Handle NVS được đóng trên mọi nhánh sau khi mở thành công.

Nếu queue kết quả đầy, `co_ket_qua` vẫn là true. Lượt tiếp theo chỉ gửi lại
kết quả đã có; không lấy việc mới và không ghi flash thêm lần nữa. Worker
poll khoảng 20 ms, stack 3072 byte, priority 3; cần đo lại tải/stack trên board.
State machine không gọi NVS trực tiếp; tuy vậy thao tác flash của ESP-IDF vẫn
có thể ảnh hưởng lịch chạy thực tế, host test không chứng minh độ trễ trên chip.

## 4. Xử lý thành công và lỗi

- Thành công đúng mã: lõi cập nhật ngưỡng hiệu lực và đóng phiên chỉnh.
- Lỗi mở/set/commit: gửi thất bại; lõi giữ ngưỡng hiệu lực trong RAM và bản nháp
  để người dùng thử lại. Log worker ghi mã lỗi NVS cụ thể.
- Kết quả sai boot/lần lưu: lõi từ chối, không đóng nhầm phiên.
- Quá hạn lưu: lõi giữ yêu cầu đang chờ và đánh dấu chưa rõ kết quả; kết quả
  hợp lệ đến muộn vẫn được xử lý.

Lỗi ghi/commit không phải cam kết rollback flash. Ví dụ ESP-IDF có lỗi
`ESP_ERR_NVS_REMOVE_FAILED` mà quá trình cập nhật có thể hoàn tất sau khi NVS
khởi tạo lại. Sau reset, ứng dụng đọc lại bản ghi hợp lệ thực tế; có thể nhận
giá trị mới dù phiên chạy trước chưa báo lưu thành công. Không tự thử lại ghi
flash khi lỗi. Tham khảo [tài liệu NVS Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.3.6/esp32/api-reference/storage/nvs_flash.html);
API đã được đối chiếu thêm với `nvs.h` của ESP-IDF 5.5.4 cài tại máy.

## Chạy kiểm thử không cần board

Kết quả triển khai: 136 kiểm tra lưu trữ đạt; tổng các bộ kiểm thử là 634.
Build ESP-IDF 5.5.4 đã đạt ở cả chế độ demo và GPIO. Chưa flash board.

Từ thư mục gốc workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/storage/run.ps1
```

Runner biên dịch storage, storage_task, control_task và control_core thật với
API giả. Log nằm trong `Gas_System/tests/storage/out/build.log` và `test.log`.
Các tình huống kiểm tra: thiếu dữ liệu, sai kiểu/phiên bản/giới hạn, lỗi init,
mở/set/commit, đóng handle, lưu/đọc qua kho mới mô phỏng restart, khôi phục
lõi Auto/OFF, queue kết quả đầy, kết quả cũ, timeout và retry sau lỗi.

Fake NVS chỉ mô hình hóa các lời gọi và lỗi được tiêm vào; không mô phỏng đầy
đủ thuật toán flash, hao mòn, mất điện giữa các bước hay phục hồi NVS.

## Khi build/chạy firmware

`CONFIG_DK_THU_NGHIEM=y` vẫn giả lập GPIO và gửi hai lệnh demo Manual/ON.
NVS là thật ở cả hai chế độ; demo không tự gửi SAVE nên không tự lưu ngưỡng.
Ở chế độ GPIO, SAVE đã ghi NVS thật, thay cho log từ chối tạm thời trước đây.
Mã boot cố định 1 còn dành cho demo cục bộ; cần thay trước khi triển khai web
và kiểm tra lệnh xuyên reset. LCD, ADC và web chưa được triển khai.

Module kế tiếp là MQ-2/ADC + sensor_task: bắt đầu bằng dữ liệu giả để kiểm tra
lọc và độ mới của mẫu, sau đó mới đo và hiệu chỉnh trên board.
