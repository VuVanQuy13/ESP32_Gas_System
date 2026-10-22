#ifndef TELEGRAM_NOTIFY_H
#define TELEGRAM_NOTIFY_H

#include "esp_err.h"
#include "telegram_notify_types.h"

/* Tao task gui Telegram. Goi mot lan luc khoi dong neu da bat trong menuconfig. */
esp_err_t telegram_notify_start(void);

/* Gui su kien khong cho. Khi worker dang xu ly, chi FIRE_CLEARED duoc giu mot lan de gui sau. */
esp_err_t telegram_notify_submit(const telegram_notify_event *event);

#endif
