#pragma once
#include "control_task.h"
/* `sensor_task_t` là bí danh của struct nội bộ được định nghĩa trong sensor_task.c. */
typedef struct sensor_task sensor_task_t;

typedef struct {
    uint64_t boot_id;
    unsigned period_ms, sample_timeout_ms;

    /* Callback ngắn, chỉ đọc mẫu thô. Ngu cảnh tồn tại đến reset. */
    esp_err_t (*read_adc)(int *sample, void *context);
    void *context;
} sensor_task_config;

/* Gọi một lần sau khi tạo dịch vụ điều khiển; task sở hữu bộ lọc và sequence. */
esp_err_t sensor_task_start(sensor_task_t **out, control_service_t *service, const sensor_task_config *setup);
