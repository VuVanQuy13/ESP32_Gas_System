#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool configured;
    int nho_nhat, lon_nhat;
    esp_err_t init_error;
} storage_store;

/* Khởi tạo struct bằng 0, gọi một lần trước khi tạo các task.
 * Sau startup chỉ storage_task được đọc/ghi kho này. Không tự xóa NVS khi lỗi. */
esp_err_t storage_initialize(storage_store *store, int nho_nhat, int lon_nhat);

/* Chỉ sửa *nguong khi đọc và kiểm tra thành công. */
esp_err_t storage_read(const storage_store *store, int *threshold);

/* ESP_OK chỉ sau set + commit thành công. Lỗi không cam kết flash được rollback. */
esp_err_t storage_write(const storage_store *store, int threshold);
