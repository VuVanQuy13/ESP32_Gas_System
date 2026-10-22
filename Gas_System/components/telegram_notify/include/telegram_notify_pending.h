#ifndef TELEGRAM_NOTIFY_PENDING_H
#define TELEGRAM_NOTIFY_PENDING_H

#include <stdbool.h>

#include "telegram_notify_types.h"

/* Giữ tối đa một event có FIRE_CLEARED khi worker đang gửi event trước đó. */
typedef struct {
    bool fire_clear_pending;
    telegram_notify_event fire_clear_event;
} telegram_notify_pending_state;

/* Event có FIRE_DETECTED hủy tin hết lửa đang chờ vì nó đã trở nên lỗi thời.
 * Trả về true khi event hiện tại được giữ để gửi sau. */
bool telegram_notify_pending_handle_busy(
    telegram_notify_pending_state *state,
    const telegram_notify_event *event);

/* Worker lấy event hết lửa đã giữ. Trả về false nếu không có event nào. */
bool telegram_notify_pending_take(
    telegram_notify_pending_state *state,
    telegram_notify_event *event);

#endif
