# Wi-Fi và web LAN: cách đọc và chạy phần mềm

Module được triển khai ngày 26/09/2026. Chưa cần board để chạy kiểm thử; kết nối Wi-Fi và WebSocket trên ESP32 cần được kiểm chứng sau khi có phần cứng.

**Bàn giao:** ưu tiên [flash-guide.md](flash-guide.md) để cấu hình Wi-Fi và phần cứng bằng JSON rồi build/nạp. Cách tạo `wifi_local.h` trực tiếp bên dưới vẫn dùng được cho build thủ công; profile bàn giao ưu tiên header sinh riêng trong build.

## Đọc source theo luồng dữ liệu

1. `Gas_System/main/Gas_System.c`: tạo mã phiên khởi động, khởi tạo dịch vụ điều khiển và các task, gọi `mang_bat_dau()`, sau đó `web_bat_dau()`. Thiếu cấu hình Wi-Fi thì các task cục bộ vẫn chạy.
2. `components/network/network.c`: khởi tạo STA, đăng ký sự kiện kết nối/mất kết nối, thử kết nối lại theo nhịp 5 giây khi không có lần kết nối đang diễn ra. Khi nhận IP, log địa chỉ mở web. Không ghi SSID/mật khẩu vào log.
3. `components/web_service/web_task/web_task.c`: phục vụ HTML/CSS/JS và endpoint `/ws`. Callback HTTP và công việc phát bản tin chạy trên cùng task HTTP server. Task `nhip_web` chỉ xếp công việc phát mỗi 200 ms; cờ atomic giới hạn một công việc đang chờ.
4. `components/web_service/web_gateway.c`: kiểm tra JSON, chống lệnh trùng, chuyển lệnh thành `dk_su_kien`, gọi `dk_gui_lenh_web()`. Gateway không sửa trạng thái điều khiển.
5. `components/app_tasks/control_task/control_task.c`: chủ sở hữu trạng thái xử lý từng lệnh, kiểm tra tuổi lệnh và phiên điều khiển rồi gọi core. Mọi thay đổi mode/relay làm tăng `phien_dieu_khien`; mẫu ADC mới chỉ tăng phiên snapshot, không tự làm cũ lệnh chỉnh ngưỡng.
6. `components/web_service/web/app.js`: nhận snapshot để hiển thị. Bấm nút tạo lệnh có phiên tương ứng; chờ phản hồi ESP32. Kéo slider chỉ là xem trước, thả slider mới gửi giá trị.

Luồng đầy đủ:

```text
Web -> WebSocket -> gateway -> queue -> control_task -> control_core
                                         |                 |
                                         +-> relay         +-> yêu cầu lưu -> storage_task
                                         |
                                         +-> snapshot -> web / LCD
Nút cứng -> button_task -> cùng queue -----+
MQ-2 -> sensor_task -> cùng chủ sở hữu ----+
```

Driver vẫn nằm riêng trong `components/drivers`; phần web không truy cập GPIO, ADC hay I2C. Task web nằm trong component `web_service` để đóng gói cùng HTTP server và tài nguyên web; thư mục `app_tasks/web_task` chỉ dẫn tới vị trí này.

## Các tình huống cần hiểu

- `QUEUED` chỉ xác nhận đã vào queue. `APPLIED` xác nhận xử lý thành công; lỗi đầu ra có kết quả riêng. Relay trên UI là đầu ra đã ra lệnh, không phải đo tiếp điểm thực tế.
- `PENDING` khi lưu chưa chứng minh NVS đã ghi xong. UI theo dõi snapshot có cùng `save_id` để hiển thị thành công, lỗi hoặc chưa rõ kết quả.
- Web và nút cứng dùng cùng bản nháp. Nếu revision thay đổi trong lúc kéo slider, UI hủy giá trị đang kéo và nhận bản nháp mới; không tự ghi đè.
- Lệnh web nằm chờ từ 1 giây trở lên bị từ chối. SET_MODE/SET_RELAY phải khớp phiên điều khiển; SET_DRAFT/SAVE_DRAFT phải khớp phiên chỉnh và revision bản nháp.
- Mất kết nối: UI khóa điều khiển, không giả số đo bằng 0, không tự phát lại lệnh. ESP32 giữ bản nháp đến hạn không thao tác; nút cứng vẫn tiếp tục được.
- Snapshot quá 1 giây được đánh dấu cũ. Trình duyệt đóng kết nối khi không nhận snapshot trong 2 giây hoặc lệnh chưa có phản hồi cuối trong 3 giây, rồi thử nối lại. Đây là các chính sách timeout phần mềm, chưa phải số liệu đo trên board.

## Kiểm thử trên PC

Tại thư mục gốc repository, chạy:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/web/run.ps1
```

Runner cần GCC host, Node.js và cJSON trong ESP-IDF; có thể truyền `-ThuMucIdf <đường-dẫn-ESP-IDF>`. Nhật ký nằm trong `Gas_System/tests/web/out/`. Các ca kiểm thử dùng source C/JS thật với API RTOS/Wi-Fi và DOM/WebSocket giả lập có điều khiển; không cần mạng hay board.

Đã kiểm tra parser, queue đầy, lệnh trùng/cũ, chỉnh chung bản nháp, lưu bất đồng bộ, reconnect, timeout, lỗi khởi tạo mạng và chu kỳ kết nối lại. Kiểm thử UI chạy bằng Node, chưa phải chạy trình duyệt thật nối ESP32.

## Khi có board

1. Sao chép `components/network/include/wifi_local.example.h` thành `wifi_local.h` trong cùng thư mục; điền `WIFI_TEN` và `WIFI_MAT_KHAU`. File cấu hình cá nhân được gitignore. Hiện hỗ trợ mạng WPA2/WPA3 Personal với SSID 1–32 byte và mật khẩu 8–63 byte.
2. Build lại. Chưa có file này thì firmware bỏ qua mạng; đây là hành vi có chủ đích. Bản build dùng thông tin Wi-Fi chứa thông tin đó trong firmware.
3. Có thể thử web trên board với `CONFIG_DK_THU_NGHIEM=y` trước: ADC, relay, LCD được giả lập, NVS vẫn thật. Sau đó mới tắt chế độ thử để dùng driver phần cứng.
4. Nạp và xem log; mở `http://<IP-trong-log>/` trên thiết bị cùng LAN. Thử thay đổi từ web và nút, lưu ngưỡng rồi reset, ngắt Wi-Fi giữa lúc chỉnh.

Ngưỡng dùng đơn vị ADC, chưa phải ppm. Chế độ phần cứng vẫn phải xác định thời gian ổn định MQ-2 trước khi Ready. Chu kỳ phát mục tiêu 200 ms không đảm bảo độ trễ tối đa; client chậm có thể trì hoãn HTTP server nhưng không trực tiếp chặn task điều khiển.

Web hiện dùng HTTP trong LAN tin cậy, kiểm tra Origin cùng Host, chưa có đăng nhập/TLS; Origin không thay thế xác thực. Tối đa 3 kết nối WebSocket. Chưa kiểm chứng radio, nhiều trình duyệt thật, lịch chạy đa lõi, điện áp và độ nhiễu cảm biến.
