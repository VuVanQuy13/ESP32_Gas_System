#pragma once
#include "control_task.h"
#include "storage.h"
/* `storage_task_t` là bí danh của struct nội bộ được định nghĩa trong storage_task.c. */
typedef struct storage_task storage_task_t;

/* Gọi một lần; kho được sao chép, dịch vụ phải tồn tại đến reset. */
esp_err_t storage_task_start(storage_task_t **out, control_service_t *service, const storage_store *store);
