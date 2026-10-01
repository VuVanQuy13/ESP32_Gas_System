#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool da_thiet_lap;
    int nho_nhat, lon_nhat;
    esp_err_t loi_khoi_tao;
} luu_kho;

/* Khởi tạo struct bằng 0, gọi một lần trước khi tạo các task.
 * Sau startup chỉ storage_task được đọc/ghi kho này. Không tự xóa NVS khi lỗi. */
esp_err_t luu_khoi_tao(luu_kho *kho, int nho_nhat, int lon_nhat);

/* Chỉ sửa *nguong khi đọc và kiểm tra thành công. */
esp_err_t luu_doc(const luu_kho *kho, int *nguong);

/* ESP_OK chỉ sau set + commit thành công. Lỗi không cam kết flash được rollback. */
esp_err_t luu_ghi(const luu_kho *kho, int nguong);
