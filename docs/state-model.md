# Trạng thái, quyền sở hữu và task

## Dữ liệu do control_task sở hữu

- mode: AUTO / MANUAL.
- sensor: WARMING / READY / FAULT; sample sequence, sampled_at, last_valid_at, valid_streak, filtered_value.
- config: MISSING / VALID / INVALID; active_threshold, config_revision.
- edit: IDLE / EDITING / SAVING; edit_id, draft_threshold, draft_revision, last_activity_at, pending_save_id.
- output: relay_command, output_error; không coi GPIO write thành bằng chứng tiếp điểm đóng.
- boot_id và state_revision: nhận diện phiên boot và thứ tự snapshot. Mọi clock kiểm tra hạn là monotonic trên ESP32.

control_core nhận state + event + now + parameters và trả state mới + yêu cầu side effect. Core không gọi GPIO, NVS, mạng, RTOS hoặc đọc thời gian thật. control_task thực hiện/điều phối side effect, ghi log và phát snapshot. Không để web/button ghi trực tiếp state.

## Các chuyển trạng thái chính

| Sự kiện | Điều kiện / kết quả |
| --- | --- |
| Boot | AUTO, WARMING, OFF, IDLE; đọc cấu hình hợp lệ hoặc báo cần cấu hình |
| Sensor sample | Kiểm tra sequence, thời điểm lấy mẫu, kết quả đọc và dải cấu hình; cập nhật streak, lọc; không nhận mẫu cũ làm mới hạn |
| Readiness achieved | Đủ warm-up và streak; READY; Auto đánh giá ngay |
| Sample invalid / stale | Reset streak; FAULT, Auto OFF; Manual giữ lệnh tay; cảnh báo khí chuyển unknown khi dữ liệu không dùng được |
| Recovery | Chuỗi mẫu mới hợp lệ; còn trong thời gian warm-up thì chưa Ready; đủ điều kiện thì đánh giá Auto |
| Enter edit | Tạo edit_id mới và draft; nếu thiếu cấu hình phải cung cấp seed nháp hợp lệ theo cấu hình UI, không tự kích hoạt ngưỡng |
| Update draft | Đúng phiên/phiên bản; kiểm tra biên; tăng draft_revision, cập nhật last_activity; active_threshold giữ nguyên |
| Edit timeout | now≥deadline và EDITING: hủy draft, giữ active threshold; lệnh đến muộn bị từ chối |
| Save request | Chụp bản nháp hợp lệ và revision, tạo save_id; không báo đã lưu |
| Save success | Chỉ nhận đúng save_id; đổi active threshold, đóng edit, Auto đánh giá lại |
| Save failure | Giữ ngưỡng cũ; trở lại EDITING với draft; cấp lại khoảng chờ chỉnh từ thời điểm kết quả |
| Web disconnected | Không thay đổi mode/relay/draft; không tự gia hạn edit timeout |

Lưu quá hạn không đồng nghĩa flash chưa ghi: báo trạng thái kết quả chưa xác định, chặn Save mới đến khi đọc đối chiếu hoặc nhận kết quả hợp lệ. Không áp dụng kết quả muộn cho một yêu cầu khác. Timeout lưu không được chặn lấy mẫu/điều khiển.

## Phân công

| Folder task | Sở hữu / việc làm | Được gọi |
| --- | --- | --- |
| sensor_task | Chu kỳ lấy mẫu, sequence, timestamp, báo lỗi | drivers/mq2 |
| button_task | Mức nút, debounce, cạnh nhấn; không lặp tự động khi giữ ở bản đầu đề xuất | drivers/buttons |
| control_task | State chính thức, timeout, điều phối kết quả | control_core, drivers/relay |
| display_task | Bản sao hiển thị, định dạng 2 dòng | drivers/lcd1602 |
| storage_task | Yêu cầu lưu đang chạy, trả kết quả có save_id | settings/NVS |
| web_task | Snapshot truyền ra, giới hạn client chậm | web_gateway |

Callback HTTP/WebSocket nhận/kiểm tra request và gửi command; không chạy luật điều khiển. Số task ứng dụng dự kiến là sáu; task mạng ESP-IDF là riêng. Chưa gán priority/stack/core affinity khi chưa đo thời gian xử lý.

## Queue và tính sống

- Thông điệp nhỏ truyền bản sao, không truyền con trỏ tới buffer callback sắp hết vòng đời.
- Queue sự kiện vào hữu hạn; nhận được vào queue chưa phải áp dụng thành công.
- Web đầy queue: trả BUSY. Nút đầy queue: lưu log mất sự kiện/đếm overflow, không âm thầm báo thành công.
- Sensor queue overflow: đánh dấu mất chuỗi bằng sequence; reset streak Ready khi có gap. Không coi chỉ mẫu mới nhất là một chuỗi liên tục.
- Snapshot LCD và web dùng mailbox riêng giữ giá trị mới nhất; không dùng một queue để mong cả hai cùng nhận.
- Storage result có kênh riêng có dung lượng cho yêu cầu đang chờ; không bỏ kết quả vì queue telemetry đầy.
- Control kiểm tra deadline trước xử lý lệnh và sau mỗi nhóm sự kiện hữu hạn; thời gian chờ queue giới hạn bởi deadline gần nhất, không chờ vô hạn khi đang cần phát hiện sensor stale.
- Lệnh và phản hồi có giới hạn tốc độ/dung lượng; client chậm không được giữ control_task. Phân bổ dung lượng cụ thể là phần triển khai cần đo, không dùng queue vô hạn.
