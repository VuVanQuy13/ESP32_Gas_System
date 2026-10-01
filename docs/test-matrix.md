# Ma trận kiểm thử G1

Cập nhật tích hợp: test control_task dùng FreeRTOS giả đạt 243 checks, bổ sung một phần T14 (queue đầy/không mất phản hồi), T17 (kênh kết quả lưu) và phân phối snapshot độc lập. Không có test mạng thật, NVS thật hoặc bộ lập lịch ESP32. Core regression giữ 123 checks PASS.

Trạng thái: đã chạy host unit test control_core (10 nhóm, 123 checks). Đây không phải kết quả pass toàn bộ hệ thống. T01–T08, T10–T13, T17–T18 đã có kiểm tra logic tương ứng; phần UI/board/storage thật vẫn NOT RUN. T09, T14–T16, T19 chưa có kiểm thử tích hợp; kiểm tra boot/edit ID chỉ là một phần T15. Xem code-walkthrough.md và log trong Gas_System/tests/control_core/out/.

| ID | Given / When | Then | Mức |
| --- | --- | --- | --- |
| T01 / R01 | Cấu hình hợp lệ, reset | AUTO/WARMING/OFF, ngưỡng phục hồi | Unit + board |
| T02 / R02 | Đủ streak nhưng chưa đủ warm-up | Chưa Ready | Unit, clock giả |
| T03 / R02 | Đủ warm-up, thiếu streak hoặc sequence gap | Chưa Ready; gap reset streak | Unit |
| T04 / R01,R04 | Vừa Ready với số đo ≥T_on | Auto ON ngay | Unit |
| T05 / R04 | READY, now bằng stale deadline | FAULT, Auto OFF, số đo không còn live | Unit |
| T06 / R03 | Manual, vượt ngưỡng hoặc lỗi sensor | Lệnh tay vẫn tác động, cảnh báo/lỗi vẫn hiện | Unit + board |
| T07 / R05 | Auto→Manual; Manual→Auto khi chưa Ready | Giữ output ở bước đầu, OFF ở bước sau | Unit |
| T08 / R06 | Web thấy revision n, nút sửa thành n+1, web SET/SAVE n | Từ chối stale; không ghi flash | Unit + protocol |
| T09 / R07 | Mất web trong EDITING, nút tiếp tục chỉnh | Draft giữ, nút sửa được, gia hạn hoạt động | Unit + integration |
| T10 / R07 | Không thao tác đến đúng edit deadline | Hủy draft; late command từ chối | Unit |
| T11 / R08 | Save thất bại | Giữ ngưỡng cũ, draft còn; không báo SAVED | Unit + storage |
| T12 / R08 | Boot với config thiếu/hỏng | OFF, cần cấu hình, Auto bị chặn | Unit + board |
| T13 / R09 | FAULT rồi chuỗi mẫu hợp lệ; một lỗi xen giữa | Chỉ Ready sau chuỗi đầy đủ; đánh giá Auto ngay | Unit |
| T14 / R10 | Queue đầy hoặc client chậm | Không báo APPLIED sai, control không chờ mạng vô hạn | Integration |
| T15 / R10 | Reconnect/reset với lệnh và snapshot cũ | Boot/session cũ bị loại; snapshot mới đồng bộ UI | Protocol |
| T16 / R06 | Gửi lại cùng ID cho lệnh tăng trong cửa sổ dedup | Chỉ tăng một lần | Unit + protocol |
| T17 / R08 | Save quá hạn, completion tới muộn | Không gán kết quả vào Save khác; reconciliation rõ | Integration |
| T18 / P1–P4 | Chuyển mode khi edit, edit khi Save, Manual thiếu config, vào vùng hysteresis | A/A/A/A đã chốt; logic có host test | Unit PASS |
| T19 / wiring | Đo nguồn/ADC/I2C, scan, hai dòng LCD, nút, reset output | So với wiring.md, ghi số đo/log thực tế | Board |

Unit tests tiêm now và sự kiện, không sleep/wait theo thời gian cảm biến thật. Mỗi test độc lập, reset state; test ngay trước/đúng/sau biên. Đối chiếu mapping R01–R10 với project-plan.md trước nghiệm thu. Không thử lỗi bằng cách tạo rò rỉ gas; dùng đầu vào giả để test logic.
