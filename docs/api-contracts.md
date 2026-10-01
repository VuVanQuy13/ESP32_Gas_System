# Hợp đồng driver và dịch vụ — thiết kế, chưa tạo header

Tên thao tác dưới đây là ranh giới trách nhiệm, chưa phải chữ ký C đã biên dịch. Driver dùng ESP-IDF để giao tiếp; control_core dùng kiểu dữ liệu độc lập ESP-IDF.

| Thành phần | Thao tác | Đầu vào | Đầu ra / lỗi | Chủ gọi |
| --- | --- | --- | --- | --- |
| mq2 | mq2_khoi_tao, mq2_doc (đã triển khai; sống đến reset) | ADC1/channel6 GPIO34, 12 bit/12 dB; điện áp chưa đo | ADC raw; lỗi config/read; đầu ra không đổi khi lỗi | sensor_task |
| buttons | init, read_levels | GPIO25/26/27/32, cấu hình kéo | Bitmask mức điện bốn nút; chưa gán chức năng | button_task |
| relay | init_off, set | GPIO23, active-high, ON/OFF | Kết quả xuất GPIO; không có phản hồi tiếp điểm | control_task; init ở startup |
| lcd1602 | lcd_khoi_tao, lcd_viet, lcd_i2c_xuat (đã triển khai) | Sơ đồ PCF cấu hình; hai dòng đúng 16 ASCII + NUL; địa chỉ chưa đo trên board | Lỗi bus/timeout/tham số; lỗi ghi buộc init lại, không truncate đầu vào driver | display_task |
| settings | load, save | Cấu hình có schema version, ngưỡng và giới hạn | VALID/MISSING/INVALID/IO_ERROR; Save chỉ thành công sau commit thành công | storage_task; load ở startup |
| control_core | initialize, process_event, next_deadline | State/event/time/parameters | State và side effects; không I/O trực tiếp | control_task hoặc unit test |
| web_gateway | start, send_snapshot, send_result | Cấu hình mạng không hardcode secret; dữ liệu đã xác nhận | Lỗi gửi/client/offline; payload không hợp lệ bị từ chối | web_task/callback |

Quy tắc chung: kiểm tra null, phạm vi, trạng thái khởi tạo; lỗi trả về rõ, không nuốt lỗi. Mỗi driver có một task sở hữu sau startup; nếu bus được chia sẻ về sau, bổ sung quyền sở hữu/khóa bus. Mọi thao tác bus có thời gian chờ hữu hạn. LCD dùng ASCII/chuỗi không dấu ở bản đầu để không giả định ROM hiển thị tiếng Việt.

Driver không biết mode, threshold, phiên chỉnh hoặc giao thức web. Khi triển khai, .c/.h của từng driver nằm trong folder đã tạo; app_tasks và drivers có CMakeLists đăng ký rõ file con. API không yêu cầu tự viết lại driver I2C/ADC nền của ESP-IDF.

Test double cần cho core: clock truyền vào, sample events giả, kết quả Save giả, bộ ghi nhận yêu cầu relay/snapshot. Test driver dùng transport GPIO/I2C/ADC giả hoặc board; không kết luận wiring đúng từ unit test.
