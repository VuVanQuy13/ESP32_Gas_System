#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "control_core.h"
#include "esp_err.h"

/* `struct control_service` là tên tag; `control_service_t` là bí danh kiểu dùng ở nơi khác. */
typedef struct control_service control_service_t;

typedef enum {
    BUTTON_ACTION_ON_INCREMENT,
    BUTTON_ACTION_OFF_DECREMENT,
    BUTTON_ACTION_TOGGLE_MODE,
    BUTTON_ACTION_EDIT_SAVE
} button_action;

/* Nút là thao tác tương đối; owner diễn giải theo trạng thái lúc xử lý.
 * Kết quả xuất qua log/snapshot, không chiếm hàng phản hồi của web. */
esp_err_t control_send_button(control_service_t *service, button_action button);

typedef struct {
    uint64_t revision;
    uint64_t control_revision; /* Chỉ đổi khi mode hoặc relay đổi, không theo telemetry. */
    control_state state;
    esp_err_t output_error; /* GPIO có thể lỗi dù lõi đã chấp nhận ý định. */
    bool storage_error;         /* Giữ lỗi Save để UI không mất thông báo khi có mẫu cảm biến mới. */
} control_snapshot;

typedef struct {
    uint64_t command_id;
    control_result result_event;
    esp_err_t output_error;
} control_response;

typedef struct {
    uint64_t boot_id, save_id;
    int threshold;
} storage_job;

typedef struct {
    /* Luật Auto/Manual, timeout và giới hạn ngưỡng của toàn hệ thống. */
    control_config policy;
    uint64_t boot_id;
    bool saved_data_valid;
    int saved_threshold;
    bool telegram_notifications_ready;

    /* Callback phải ngắn, không chờ mạng/LCD/flash. Không gọi lại API dịch vụ.
     * Được gọi OFF lúc khởi tạo, sau đó chỉ từ control_task.
     * ngu_canh phải tồn tại suốt vòng đời dịch vụ, không trỏ vào biến tạm. */
    esp_err_t (*set_relay)(bool on, void *context);
    void *context;
} control_task_config;

/* Gọi một lần trong startup trước khi công bố con trỏ cho các task khác.
 * Dịch vụ sống đến reset, không hỗ trợ xóa nóng khi producer còn hoạt động. */
esp_err_t control_task_start(control_service_t **service, const control_task_config *setup);

/* API dành cho task, không dùng trong ISR. ESP_OK chỉ nghĩa đã xếp hàng.
 * command_id khác 0 cho lệnh; sensor dùng control_send_sample(), không tạo phản hồi lệnh.
 * Bên định tuyến cấp command_id duy nhất, đọc phản hồi và chuyển đúng client. */
esp_err_t control_send_command(control_service_t *service, uint64_t command_id, const control_event *event);

/* Gateway duy nhất gọi; owner kiểm tra tuổi queue <1000 ms và revision mode/relay. */
esp_err_t control_send_web_command(
    control_service_t *service, uint64_t command_id, const control_event *event, uint64_t control_revision);
esp_err_t control_send_sample(control_service_t *service, const control_event *event);
esp_err_t control_read_response(control_service_t *service, control_response *response);

/* Hai mailbox độc lập, đọc không lấy mất bản chụp của giao diện còn lại. */
esp_err_t control_read_lcd_snapshot(control_service_t *service, control_snapshot *snapshot);
esp_err_t control_read_web_snapshot(control_service_t *service, control_snapshot *snapshot);
esp_err_t control_take_storage_job(control_service_t *service, storage_job *job);

/* Chỉ storage_task gọi; gateway không được giả mạo kết quả lưu. */
esp_err_t control_report_storage_result(control_service_t *service, const control_event *event);

#endif
