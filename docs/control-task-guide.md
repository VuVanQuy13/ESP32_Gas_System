# Triển khai control_task và cách đọc code

## Phần đã viết và giới hạn kiểm chứng

Đã có control_task dùng API FreeRTOS thật, CMake component app_tasks, header công khai và demo app_main. Host test biên dịch chính file control_task.c với lớp mô phỏng queue/semaphore/task/clock để gọi từng lượt xác định. Không sao chép logic task sang chương trình test khác.

Kết quả: 243 phép kiểm tra tích hợp mô phỏng PASS; 123 phép kiểm tra core PASS. Mô phỏng không kiểm chứng SMP, bộ lập lịch, thời gian thực, stack hoặc driver điện. ESP-IDF tìm thấy tại bộ cài 5.5.4 trên máy nhưng ba lần chuẩn bị môi trường gặp lỗi trước compiler (execution policy, registry Python trả null, stderr tiến độ bị PowerShell coi là lỗi). Đã dừng thử môi trường trong lượt này. Script build được sửa cách xử lý stderr sau đó nhưng chưa chạy lại; chưa có kết quả build firmware thành công.

## 1. Các file chính

- `components/app_tasks/control_task/control_task.h`: API để sensor/button/web/storage/UI giao tiếp với dịch vụ, không cho truy cập state nội bộ.
- `control_task.c`: giữ state, queue, gọi core và phân phối kết quả.
- `control_task_noi_bo.h`: một lượt xử lý và phép tính timeout cho test. Không phải API dành cho các task khác.
- `main/Gas_System.c`: demo không GPIO thật, minh họa khởi tạo và gửi hai lệnh.
- `main/Kconfig.projbuild`: `CONFIG_DK_THU_NGHIEM`, bật mặc định để demo; tắt sẽ chỉ báo chờ tích hợp driver.

Tên file giữ theo cấu trúc đã thống nhất; hàm/biến/chú thích dùng tiếng Việt. Một số biến cục bộ d/e/c viết gọn lần lượt là dịch vụ/sự kiện/cấu hình.

## 2. dk_bat_dau tạo những gì?

Hàm nhận cấu hình core, kết quả đọc ngưỡng từ storage, boot ID và callback xuất relay. Nó kiểm tra core config, tạo các queue, yêu cầu output OFF, phát snapshot ban đầu rồi tạo task. Nếu lỗi cấp phát hoặc tạo task, giải phóng các tài nguyên đã tạo và trả lỗi. Nếu output OFF thất bại thì không khởi động dịch vụ.

Trả về con trỏ `dk_dich_vu *`. Nội dung struct chỉ nằm trong .c, producer không thể trực tiếp sửa trạng thái. Khởi tạo một lần trước các producer; chưa hỗ trợ dừng/xóa dịch vụ đang chạy, vòng đời đến reset.

Priority ban đầu 5, stack 4096 BYTE theo quy ước ESP-IDF, không ghim core. Đây là giá trị khởi đầu cần đo stack/tải, không phải cấu hình đã tối ưu. Vòng lặp xử lý một lượt hữu hạn rồi nhường một tick khi có việc liên tục.

## 3. Tại sao có nhiều queue?

| Kênh | Dung lượng | Vai trò |
| --- | ---: | --- |
| dau_vao | 16 | Lệnh và mẫu, sao chép theo giá trị |
| phan_hoi | 16 | Kết quả lệnh có ma_lenh để trả đúng người gửi |
| lcd | 1 | Mailbox giữ snapshot mới nhất cho LCD |
| web | 1 | Mailbox độc lập cho web |
| viec_luu | 1 | Yêu cầu lưu có ma_lan_luu và ngưỡng |
| ket_qua_luu | 1 | Kết quả storage, không bị queue mẫu/lệnh đầy chặn |

Các API producer không chờ: queue đầy trả ESP_ERR_TIMEOUT và ghi log. `dk_gui_lenh` thành công chỉ nghĩa đã nhận vào queue; kết quả core đọc qua `dk_doc_phan_hoi`.

Trước khi nhận lệnh, dịch vụ lấy một suất từ counting semaphore `cho_phan_hoi`. Suất chỉ được trả khi consumer đọc phản hồi hoặc khi không đưa được lệnh vào queue. Vì mỗi lệnh được nhận đã có chỗ phản hồi, control_task không phải chờ web để ghi kết quả. Web không đọc nữa thì các lệnh mới bị từ chối; timeout và mẫu vẫn được xử lý. Một task định tuyến duy nhất sẽ đọc phản hồi rồi chuyển đến đúng nguồn bằng ma_lenh; ID và client mapping/chống lặp chưa triển khai ở gateway.

Snapshot dùng overwrite và peek. LCD đọc không lấy mất snapshot web. `phien_ban` tăng mỗi lần phát, bao gồm nhịp timeout; không dùng nó thay `phien_ban_nhap` để kiểm tra chỉnh ngưỡng.

## 4. Một lượt điều khiển

`dk_chay_mot_luot` chỉ do control_task gọi (test đơn luồng được gọi trực tiếp):

1. Tạo sự kiện kiểm tra thời gian, gọi core.
2. Nếu có kết quả lưu, xử lý một kết quả.
3. Nếu có đầu vào, xử lý một sự kiện và gửi phản hồi nếu có ma_lenh.
4. Phát snapshot riêng cho LCD/web.

Mỗi lượt hữu hạn nên dù lệnh đến liên tục vẫn kiểm tra deadline. Khi hết việc, `dk_so_tick_cho` lấy deadline core, giới hạn chờ 100 ms và làm tròn lên tick. Task chờ notification; producer gửi queue xong mới notify. Notification là tín hiệu đánh thức, dữ liệu thật vẫn nằm trong queue.

Deadline thực tế có độ trễ do tick, thời gian xử lý, ghi log và lập lịch; không tuyên bố hard realtime. Ghi log UART có thể ảnh hưởng timing khi flood; cần đo trên board và giảm mức log cho telemetry ở giai đoạn tải.

## 5. Kết quả bị từ chối vẫn có thể đổi relay

Core kiểm tra timeout trước khi kiểm tra lệnh. Nếu mẫu vừa hết hạn, Auto phải OFF dù lệnh web đang sai revision. Hàm `xu_ly` luôn xem `relay_thay_doi` trong kết quả, không chỉ khi status thành công.

Callback `xuat_relay(bool bat, void *ngu_canh)` phải ngắn, không chờ I2C/mạng/flash. Khi callback trả lỗi, snapshot/response giữ `loi_dau_ra`; ý định relay và kết quả xuất chân là hai thông tin khác nhau. Lượt sau thử lại đầu ra mong muốn. Gateway không được hiển thị đầu ra đã thực hiện khi loi_dau_ra khác ESP_OK.

Demo callback chỉ ghi log, không bật chân 23. Khi có driver relay sẽ thay callback bằng thao tác GPIO.

## 6. Lưu ngưỡng không chặn điều khiển

Core phát `can_luu`; dịch vụ đưa `{ma_khoi_dong, ma_lan_luu, nguong}` vào viec_luu. storage_task tương lai gọi `dk_nhan_viec_luu`, ghi NVS, sau đó `dk_bao_ket_qua_luu`.

Kết quả Save không đi qua API lệnh web. Nếu queue kết quả lưu đang đầy, storage phải giữ và gửi lại; không được bỏ kết quả. Kết quả cuối đến core cập nhật snapshot với ma_lan_luu, trạng thái chỉnh và ngưỡng. Phản hồi ban đầu của SAVE chỉ là yêu cầu được nhận, không phải ghi flash xong.

Nếu chưa giao được yêu cầu lưu vì queue viec_luu đầy, dịch vụ gửi lỗi nội bộ về core và giữ ngưỡng cũ/bản nháp. Nếu đã giao rồi mà quá hạn thì không tự kết luận ghi thất bại: core giữ trạng thái chưa rõ, đợi kết quả/reconciliation.

## 7. Cách nối producer/consumer sau này

- sensor_task gọi `dk_gui_mau` với timestamp lúc đo và sequence tăng trên mỗi lần lấy mẫu, kể cả lần không gửi được. Gap sẽ reset chuỗi Ready.
- button_task gửi ý định nút; phần chuyển nút tăng/giảm theo ngữ cảnh còn cần bổ sung tại owner, không đọc snapshot rồi sửa state trực tiếp.
- gateway gọi `dk_gui_lenh` sau kiểm tra schema/quyền truy cập; không chấp nhận ma_lenh=0 hoặc sự kiện giả storage.
- display_task gọi `dk_doc_lcd`, hiển thị trạng thái cảm biến/lỗi và ngưỡng.
- web_task gọi `dk_doc_web`; consumer phản hồi lệnh gọi `dk_doc_phan_hoi` rồi định tuyến theo ma_lenh.
- storage_task gọi API queue lưu như mục 6.

Chưa có driver, NVS, web, debounce, lọc ADC, chống lệnh gửi lại hoặc thời hạn command gateway. Không diễn giải test queue thành đã hoàn thiện những thành phần đó.

## 8. Chạy thử

Từ workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/control_core/run.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/control_task/run.ps1
```

Log nằm trong thư mục out của từng bộ test. Dữ liệu giả đi qua đúng hàm runtime; fake FreeRTOS chỉ thay transport và clock.

Với terminal ESP-IDF đã hoạt động:

```powershell
cd Gas_System
idf.py -B build_control_task build
```

Chưa flash trong lượt này. Demo gửi MODE=MANUAL rồi RELAY=ON, callback ghi `gpio=khong_su_dung`; sau hạn dữ liệu sensor báo lỗi nhưng Manual giữ ý định ON đúng yêu cầu. Demo chỉ gửi hai lệnh và chưa có consumer phản hồi; thêm producer thực phải thêm consumer định tuyến trước. Tham số giả/boot_id cố định chỉ cho demo không mạng, tuyệt đối không dùng như calibration hoặc session ID sản phẩm.

Tài liệu đối chiếu API và đơn vị stack: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/freertos_idf.html
