# Đặc tả chức năng — G1

Trạng thái: bản thiết kế để triển khai từng phần. Các yêu cầu R01–R10 đã chốt nằm trong project-plan.md. Phép đo G0 chưa thực hiện; tài liệu này không chứng nhận phần cứng. Không thêm cloud, RGB hoặc kênh relay mới.

## Yêu cầu bắt buộc

- Reset vào Auto, relay OFF; phục hồi ngưỡng lưu hợp lệ.
- Chỉ Ready sau thời gian ổn định và chuỗi mẫu mới hợp lệ. Không dùng điều kiện số đo thấp để cho Ready.
- Auto đánh giá ngưỡng; mất dữ liệu hợp lệ phải OFF và báo lỗi riêng. Manual vẫn điều khiển relay khi cảm biến lỗi và vẫn hiển thị lỗi.
- Auto→Manual giữ relay; Manual→Auto đánh giá ngay hoặc OFF nếu chưa Ready.
- Hai giao diện sửa cùng bản nháp; Save thành công mới đóng phiên. Save lỗi giữ ngưỡng cũ và bản nháp.
- Mất web không hủy bản nháp ngay. Hết hạn không thao tác từ cả hai phía thì hủy; reconnect lấy trạng thái từ ESP32.
- Cấu hình thiếu/hỏng: khởi động OFF và báo cần cấu hình, chặn Auto. Chính sách Manual trong trường hợp này còn ở bảng quyết định dưới.
- Không có cơ chế phản hồi tiếp điểm: trường relay là lệnh đầu ra, không phải phép đo relay vật lý.

## Hành vi đã chốt

| Mã | Option A — khuyến nghị | Option B — đánh đổi |
| --- | --- | --- |
| P1 | Khóa đổi mode khi chỉnh/lưu để ý nghĩa nút nhất quán | Cho đổi mode khi chỉnh; tăng tổ hợp cần test |
| P2 | Khi đang lưu, khóa sửa; giữ ngưỡng cũ đến khi lưu thành công | Cho sửa tiếp, cần thêm phiên bản bản nháp trong lúc lưu |
| P3 | Thiếu cấu hình vẫn cho Manual điều khiển LED, luôn báo cần cấu hình | Chặn relay cả Manual đến khi có cấu hình |
| P4 | Khi vừa vào Auto/Ready trong dải hysteresis, bắt đầu OFF | Giữ đầu ra cũ; có thể tiếp tục ON sau chuyển từ Manual |

Người dùng đã chọn A/A/A/A cho P1–P4. Các lựa chọn A là yêu cầu đã chốt và được triển khai trong control_core. Cột B chỉ giữ để ghi lại phương án so sánh.

## Tham số

Đơn vị bản đầu đã triển khai: ADC raw 12-bit, trung bình tối đa bốn mẫu; calibration ADC/mV không phải calibration khí. Ngưỡng so với dữ liệu đã lọc, cùng đơn vị. Warm-up, số mẫu Ready, độ chênh hysteresis và ngưỡng phù hợp module phải có căn cứ thực nghiệm.

Chính sách thử nghiệm sensor_task: loại mẫu lỗi/chạm biên 0 hoặc 4095,
reset bộ lọc khi lỗi/gián đoạn/quá hạn hoặc queue đầy. Đọc khoảng 100 ms/lần,
hạn dữ liệu 500 ms. Chưa xác định warm-up thật thì giữ chưa Ready qua
CONFIG_MQ2_ON_DINH_GIAY=0. Đây không phải chẩn đoán chắc chắn hỏng/đứt dây.
Xem sensor-guide.md cho hành vi và giới hạn; chưa xác nhận trên board.

Các tham số test được truyền vào cấu hình: sample_period, sensor_stale_timeout, warmup_duration, ready_sample_count, debounce_duration, edit_timeout, threshold_min/max/step, hysteresis_gap, storage_deadline. Phải kiểm tra >0 và quan hệ hợp lệ, không dùng thông số test như cấu hình cảm biến thực.

Ngưỡng bật T_on và ngưỡng tắt T_off = T_on − gap. Quy tắc core: x≥T_on bật; x≤T_off tắt; ở giữa giữ trạng thái Auto đã xác lập. P4 đã chốt: lần đầu vào Auto/Ready trong khoảng giữa bắt đầu OFF.

## Bốn nút đề xuất

| Nút | Bình thường | Chỉnh ngưỡng |
| --- | --- | --- |
| ON/+ | Yêu cầu ON trong Manual | Tăng một bước |
| OFF/− | Yêu cầu OFF trong Manual | Giảm một bước |
| MODE | Yêu cầu đổi Auto/Manual | Theo P1 |
| EDIT/SAVE | Mở bản nháp | Save bản nháp |

Web dùng tên lệnh rõ ràng; nút MODE vật lý được diễn giải trong control_task. Không nhân bản logic chế độ trong button_task.
