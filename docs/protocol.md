# Đồng bộ web/phần cứng — command v1 và state snapshot v2 đã triển khai

## Hợp đồng hiện tại (22/10/2026)

Phần này thay thế đề xuất lịch sử phía dưới. HTTP phục vụ `/`, `/app.js`, `/style.css`; WebSocket `/ws` truyền JSON text không phân mảnh. Lệnh client dùng schema v1; snapshot trạng thái ESP32 dùng schema v2 để bổ sung trạng thái cảm biến lửa. Tối đa 3 client, mỗi lệnh không quá 384 byte. Kiểm tra Origin cùng Host; chưa có xác thực/TLS, chỉ dành cho LAN tin cậy.

Lệnh là object phẳng gồm `v:1`, `id` nguyên 1..2147483647, `type`, `boot`, cộng đúng các trường trong bảng. Không nhận khóa thừa/trùng, object/array lồng hoặc chuỗi escape. `boot`, `control`, `edit`, `revision` là chuỗi đúng 16 chữ số hex thường; không truyền uint64 bằng Number JavaScript.

| type | Trường bổ sung | Ý nghĩa |
| --- | --- | --- |
| SET_MODE | control, value | value 0 Auto, 1 Manual; kiểm tra phiên điều khiển |
| SET_RELAY | control, value | value 0 tắt, 1 bật; chỉ khi core cho phép |
| BEGIN_EDIT | Không | Bắt đầu chỉnh chung bản nháp |
| SET_DRAFT | edit, revision, value | Đặt giá trị tuyệt đối, trong biên cấu hình |
| SAVE_DRAFT | edit, revision | Yêu cầu lưu đúng bản nháp đã thấy |

Không có ADJUST_DRAFT/GET_SNAPSHOT trên wire. UI tính +/-10 rồi gửi SET_DRAFT; ESP32 chủ động phát snapshot, mục tiêu 200 ms. Mọi lệnh web chờ trong queue >=1000 ms bị từ chối. Control revision tăng khi mode hoặc relay đổi; telemetry không làm tăng revision này. Core kiểm tra boot và phiên chỉnh/revision bản nháp.

Ví dụ (thay boot/control bằng snapshot hiện tại):

```json
{"v":1,"id":1,"type":"SET_MODE","boot":"0000000000000001","control":"0000000000000001","value":1}
```

Phản hồi gồm `type:"result"`, `id`, `status`, `code`, `save_id`. Status: INVALID, ID_CONFLICT, OLD_ID, BUSY, QUEUED, REJECTED, OUTPUT_ERROR, PENDING, APPLIED. Code của phản hồi enqueue là esp_err_t; code kết quả xử lý là dk_ma_ket_qua. OUTPUT_ERROR cho biết đầu ra lỗi dù core có thể trả mã thành công; xem relay_error trong snapshot.

PENDING chỉ xác nhận đã yêu cầu lưu. Không có ACK SAVED riêng: theo dõi snapshot cùng save_id, editing/config/save_error/unknown để xác định tiến trình và kết quả lưu. QUEUED không đồng nghĩa APPLIED. Web server là consumer duy nhất của queue phản hồi.

Mỗi socket giữ đúng lệnh gần nhất và phản hồi gần nhất: cùng id và JSON giống từng byte trả cache; cùng id nhưng khác byte trả ID_CONFLICT; id thấp hơn trả OLD_ID; khi còn lệnh chờ thì id mới trả BUSY. Cache bị xóa khi reconnect; không hứa exactly-once qua disconnect/reset. UI không tự retry lệnh sau reconnect.

Snapshot `type:"state"`, `v:2` gồm:

- Chuỗi hex: boot, seq (thứ tự snapshot), control, edit, revision, save_id.
- mode (0 Auto/1 Manual), sensor (0 chờ/1 sẵn sàng/2 lỗi), relay, relay_error, save_error.
- threshold, draft, editing (0 không chỉnh/1 chỉnh/2 đang lưu), unknown, config.
- value (ADC hoặc null), stale, minimum, maximum, step=10.
- `fire_valid`, `fire`, `fire_confirming`: lần lượt cho biết kết quả D0 có hợp lệ không, có phát hiện lửa không, và đang chờ đủ 3 giây mẫu hợp lệ báo hết lửa để gửi thông báo Telegram hay không.

Không công bố ppm. Mẫu chưa sẵn sàng/cũ hoặc snapshot cũ trả value=null; UI không kết luận an toàn từ lỗi. UI bỏ snapshot seq thấp hơn trong cùng boot; đổi boot thì bỏ cache cũ. Bản nháp chỉ chính thức sau snapshot ESP32; xung đột lúc kéo slider hủy phần xem trước.

## Đề xuất lịch sử (không dùng làm schema hiện tại)

HTTP phục vụ UI; WebSocket truyền command/result/snapshot. Đây là hợp đồng thiết kế, chưa có endpoint hoạt động. Endpoint dự kiến `/` và `/ws`; phạm vi LAN, không hardcode thông tin Wi-Fi vào repo.

## Envelope

Command chứa protocol_version, boot_id, client_id, command_id, type và payload. Giới hạn kích thước, độ dài ID, số client và lưu lượng phải được đặt hữu hạn khi triển khai. Sai kiểu, thiếu trường, số không hữu hạn hoặc ngoài biên bị từ chối trước khi vào queue. ID chỉ dùng nhận diện, không thay thế xác thực; chính sách truy cập LAN/origin còn cần xác định trước G5.

| Lệnh | Nội dung | Điều kiện |
| --- | --- | --- |
| SET_MODE | AUTO hoặc MANUAL | Theo state và P1 trong spec |
| SET_RELAY | ON hoặc OFF | Manual; thiếu cấu hình theo P3 |
| BEGIN_EDIT | Không có threshold | IDLE; tạo phiên trên ESP32 |
| SET_DRAFT | edit_id, expected_revision, value | Đúng phiên/revision, hợp lệ |
| ADJUST_DRAFT | edit_id, expected_revision, số bước | Web dùng revision; nút vật lý được diễn giải trên state hiện tại |
| SAVE_DRAFT | edit_id, expected_revision | Lưu đúng bản nháp đã nhìn thấy |
| GET_SNAPSHOT | Không | Trả trạng thái đầy đủ |

Các lệnh thay đổi mode/relay cũng cần giới hạn tuổi lệnh trên đồng hồ nhận của ESP32 và kiểm tra điều kiện mode tại lúc xử lý. Không tự phát lại lệnh UI sau reconnect. Khi cần tránh lệnh relay cũ ghi đè cùng mode, bổ sung control_revision độc lập telemetry vào hợp đồng trước G5.

## Kết quả và snapshot

- Enqueued chỉ là nhận vào hàng đợi. Result cuối: APPLIED, SAVED hoặc REJECTED kèm lý do như INVALID_ARGUMENT, WRONG_MODE, STALE_REVISION, EDIT_EXPIRED, BUSY, STORAGE_ERROR.
- Save có thể có phản hồi PENDING; chỉ SAVED khi commit đã thành công. Quá hạn lưu chưa rõ kết quả dùng UNKNOWN, không giả báo STORAGE_ERROR có nghĩa chưa ghi.
- Snapshot: boot_id, state_revision, mode, sensor health, số đo/đơn vị/tuổi mẫu, threshold active, edit status/id/revision/draft, relay_command, lỗi/config status. Không có mẫu thì value=null, không giả 0.
- state_revision dùng thứ tự snapshot; draft_revision chỉ thay đổi khi bản nháp thay đổi. Dữ liệu ADC mới không làm stale lệnh chỉnh ngưỡng.
- Sau reconnect hoặc boot_id đổi: bỏ cache UI, lấy snapshot đầy đủ; không dùng revision của phiên cũ.
- Khi xung đột: trả revision mới và snapshot; UI hiển thị thay đổi, không tự gửi lại giá trị cũ để ghi đè.
- Dedup theo boot_id/client_id/command_id trong cửa sổ hữu hạn được công bố khi triển khai. Cùng ID nhưng payload khác bị từ chối. Lệnh tăng không được tự retry ngoài cửa sổ; không hứa exactly-once xuyên reset.

UI được hiển thị vị trí slider đang kéo dạng pending, nhưng chỉ ghi nhận draft chính thức sau phản hồi ESP32. Chỉ một cập nhật slider đang chờ trên mỗi client là cách đề xuất để giảm stale command; không hạn chế nút cứng sửa xen kẽ.
