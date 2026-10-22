#ifndef CONTROL_CORE_H
#define CONTROL_CORE_H

#include <stdbool.h>
#include <stdint.h>

/* C thuần: bên gọi cung cấp thời gian mili giây tăng đơn điệu và sở hữu trạng thái.
 * Chỉ gọi từ một tác vụ. Kiểm tra giao thức/chống lệnh lặp nằm ngoài lõi. */
/* Chế độ quyết định ai được phép điều khiển relay. */
typedef enum {
    CONTROL_MODE_AUTO,
    CONTROL_MODE_MANUAL
} control_mode;

/* Tình trạng dữ liệu MQ-2, dùng để chặn Auto khi chưa an toàn. */
typedef enum {
    SENSOR_WARMING_UP,
    SENSOR_READY,
    SENSOR_ERROR
} sensor_status;

/* Tiến trình chỉnh ngưỡng chung giữa nút cứng và web. */
typedef enum {
    EDIT_IDLE,
    EDIT_ACTIVE,
    EDIT_SAVING
} edit_status;

typedef enum {
    CONTROL_EVENT_CHECK_TIMEOUTS,
    CONTROL_EVENT_NEW_SAMPLE,
    CONTROL_EVENT_SET_MODE,
    CONTROL_EVENT_SET_RELAY,
    CONTROL_EVENT_BEGIN_EDIT,
    CONTROL_EVENT_SET_DRAFT,
    CONTROL_EVENT_SAVE_DRAFT,
    CONTROL_EVENT_SAVE_OK,
    CONTROL_EVENT_SAVE_FAILED
} control_event_type;

typedef enum {
    CONTROL_OK,
    CONTROL_INVALID,
    CONTROL_TIME_ERROR,
    CONTROL_WRONG_MODE,
    CONTROL_BUSY,
    CONTROL_STALE,
    CONTROL_NO_EDIT_SESSION,
    CONTROL_STORAGE_ERROR,
    CONTROL_ID_EXHAUSTED
} control_result_code;

/* Su kien can bao do control_task xu ly; core chi quyet dinh chinh sach. */
typedef uint32_t control_notification_flags;
enum {
    CONTROL_NOTIFICATION_NONE = 0,
    CONTROL_NOTIFICATION_GAS_ALARM = 1U << 0,
    CONTROL_NOTIFICATION_GAS_RECOVERY = 1U << 1,
    CONTROL_NOTIFICATION_FIRE_DETECTED = 1U << 2,
    CONTROL_NOTIFICATION_FIRE_CLEARED = 1U << 3
};

/* Các giá trị chính sách được main truyền vào một lần khi khởi động. */
typedef struct {
    uint64_t warmup_time_ms, data_timeout_ms, edit_timeout_ms, save_timeout_ms;
    uint32_t required_sample_count;
    int minimum_threshold, maximum_threshold, threshold_hysteresis, initial_draft_threshold;
} control_config;

/* Một thông điệp gửi tới lõi điều khiển. Không task nào sửa state trực tiếp. */
typedef struct {
    control_event_type type;
    uint64_t boot_id;              /* Sự kiện phải thuộc lần khởi động này. */
    uint64_t sequence, sample_time_ms; /* Thời điểm lấy mẫu, không phải lúc nhận. */
    bool is_valid;                        /* Đọc và kiểm tra chất lượng mẫu thành công. */
    bool fire_read_valid;                 /* Cho biet D0 doc thanh cong; loi khong phai la xac nhan het lua. */
    bool fire_detected;                   /* Trang thai D0 khi fire_read_valid=true. */
    int value;                        /* Mẫu thô, bản nháp, chế độ hoặc relay 0/1. */
    uint64_t edit_session_id, expected_revision, save_id;
} control_event;

/* Toàn bộ trạng thái sống; chỉ control_task được thay đổi cấu trúc này. */
typedef struct {
    bool initialized;
    control_config config;
    uint64_t boot_id, boot_time_ms, current_time_ms;
    control_mode mode;
    sensor_status sensor_status;
    edit_status edit_status;
    bool relay_on, configuration_valid, has_sample, fire_read_valid, fire_detected,
        fire_notification_active;
    bool fire_clear_confirmation_active;
    int threshold, sample, draft;
    uint32_t consecutive_sample_count;
    bool gas_alarm_active; /* Da phat canh bao, chua xac nhan du dieu kien hoi phuc. */
    uint32_t consecutive_recovery_sample_count; /* So mau hop le lien tiep duoi ranh gioi hoi phuc. */
    uint64_t sample_sequence, sample_time_ms;
    uint64_t fire_clear_confirmation_start_ms;
    uint64_t edit_session_id, draft_revision, last_edit_time_ms;
    uint64_t save_id, save_start_time_ms;
    bool save_result_unknown;
} control_state;

/* Mỗi lần xử lý trả bản ghi có cấu trúc, kể cả khi từ chối lệnh.
 * Tác vụ điều phối phải ghi nhật ký và gửi yêu cầu lưu đúng một lần. */
typedef struct {
    control_result_code result_code;
    uint64_t event_time_ms;
    control_event_type event;
    bool relay_changed, relay_on;
    bool needs_save;
    int threshold_to_save;
    uint64_t save_id;
    control_notification_flags notifications; /* Co the co nhieu chuyen trang thai trong cung mot su kien. */
    int notification_sample, notification_threshold;
    control_mode mode;
    sensor_status sensor_status;
    edit_status edit_status;
    bool save_result_unknown;
} control_result;

/* Khởi tạo state ở Auto, relay tắt và cảm biến đang warm-up. */
control_result_code control_initialize(control_state *state,
    const control_config *config,
    uint64_t boot_id,
    uint64_t current_time_ms,
    bool saved_data_valid,
    int saved_threshold);
/* Áp dụng đúng một event và trả về các side effect cần thực hiện bên ngoài lõi. */
control_result_code control_process_event(control_state *state,
    const control_event *event,
    uint64_t current_time_ms,
    control_result *result_event);

/* UINT64_MAX: không có hạn chờ hoặc hạn vượt miền biểu diễn. */
uint64_t control_next_deadline(const control_state *state);

#endif
