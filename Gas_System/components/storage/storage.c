#include "storage.h"
#include <stdint.h>
#include <stddef.h>
#include "nvs.h"
#include "nvs_flash.h"
/* Một key u64: 16 bit magic, 16 bit phiên bản, 32 bit ngưỡng.
 * Không lưu struct C để tránh phụ thuộc padding. NVS cung cấp CRC nội bộ. */
#define DINH_DANG (UINT64_C(0x4D510001) << 32)

esp_err_t luu_khoi_tao(luu_kho *k, int nho, int lon)
{
    if (!k || nho < 0 || lon < nho)
        return ESP_ERR_INVALID_ARG;
    if (k->da_thiet_lap)
        return ESP_ERR_INVALID_STATE;
    k->da_thiet_lap = true;
    k->nho_nhat = nho;
    k->lon_nhat = lon;
    k->loi_khoi_tao = nvs_flash_init();
    return k->loi_khoi_tao;
}

static esp_err_t kiem_tra(const luu_kho *k)
{
    if (!k)
        return ESP_ERR_INVALID_ARG;
    if (!k->da_thiet_lap)
        return ESP_ERR_INVALID_STATE;
    return k->loi_khoi_tao;
}

esp_err_t luu_doc(const luu_kho *k, int *nguong)
{
    if (!nguong)
        return ESP_ERR_INVALID_ARG;
    esp_err_t loi = kiem_tra(k);
    if (loi != ESP_OK)
        return loi;
    nvs_handle_t h;
    loi = nvs_open("gas_cfg", NVS_READONLY, &h);
    if (loi != ESP_OK)
        return loi;
    uint64_t ban = 0;
    loi = nvs_get_u64(h, "nguong_v1", &ban);
    nvs_close(h);
    if (loi != ESP_OK)
        return loi;
    uint32_t gia_tri = (uint32_t)ban;
    if ((ban & UINT64_C(0xFFFFFFFF00000000)) != DINH_DANG || gia_tri < (uint32_t)k->nho_nhat ||
        gia_tri > (uint32_t)k->lon_nhat)
        return ESP_ERR_INVALID_STATE;
    *nguong = (int)gia_tri;
    return ESP_OK;
}

esp_err_t luu_ghi(const luu_kho *k, int nguong)
{
    esp_err_t loi = kiem_tra(k);
    if (loi != ESP_OK)
        return loi;
    if (nguong < k->nho_nhat || nguong > k->lon_nhat)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    loi = nvs_open("gas_cfg", NVS_READWRITE, &h);
    if (loi != ESP_OK)
        return loi;
    loi = nvs_set_u64(h, "nguong_v1", DINH_DANG | (uint32_t)nguong);
    if (loi == ESP_OK)
        loi = nvs_commit(h);
    nvs_close(h);
    return loi;
}
