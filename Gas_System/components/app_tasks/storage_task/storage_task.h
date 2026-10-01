#pragma once
#include "control_task.h"
#include "storage.h"
typedef struct luu_tac_vu luu_tac_vu;

/* Gọi một lần; kho được sao chép, dịch vụ phải tồn tại đến reset. */
esp_err_t luu_bat_dau(luu_tac_vu **ra, dk_dich_vu *dich_vu, const luu_kho *kho);
