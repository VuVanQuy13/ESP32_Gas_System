#include "storage.h"
#include <stdint.h>
#include <stddef.h>
#include "nvs.h"
#include "nvs_flash.h"
/* Một key u64: 16 bit magic, 16 bit phiên bản, 32 bit ngưỡng.
 * Không lưu struct C để tránh phụ thuộc padding. NVS cung cấp CRC nội bộ. */
#define DINH_DANG (UINT64_C(0x4D510001) << 32)

esp_err_t storage_initialize(storage_store *result, int minimum, int maximum)
{
    if (!result || minimum < 0 || maximum < minimum)
        return ESP_ERR_INVALID_ARG;
    if (result->configured)
        return ESP_ERR_INVALID_STATE;
    result->configured = true;
    result->nho_nhat = minimum;
    result->lon_nhat = maximum;
    result->init_error = nvs_flash_init();
    return result->init_error;
}

static esp_err_t kiem_tra(const storage_store *result)
{
    if (!result)
        return ESP_ERR_INVALID_ARG;
    if (!result->configured)
        return ESP_ERR_INVALID_STATE;
    return result->init_error;
}

esp_err_t storage_read(const storage_store *result, int *threshold)
{
    if (!threshold)
        return ESP_ERR_INVALID_ARG;

    esp_err_t error = kiem_tra(result);
    if (error != ESP_OK)
        return error;
    
    nvs_handle_t handle;
    error = nvs_open("gas_cfg", NVS_READONLY, &handle);
    if (error != ESP_OK)
        return error;
    uint64_t stored_value = 0;
    error = nvs_get_u64(handle, "nguong_v1", &stored_value);
    nvs_close(handle);
    if (error != ESP_OK)
        return error;
    uint32_t value = (uint32_t)stored_value;
    if ((stored_value & UINT64_C(0xFFFFFFFF00000000)) != DINH_DANG || value < (uint32_t)result->nho_nhat ||
        value > (uint32_t)result->lon_nhat)
        return ESP_ERR_INVALID_STATE;
    *threshold = (int)value;
    return ESP_OK;
}

esp_err_t storage_write(const storage_store *result, int threshold)
{
    esp_err_t error = kiem_tra(result);
    if (error != ESP_OK)
        return error;
    if (threshold < result->nho_nhat || threshold > result->lon_nhat)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    error = nvs_open("gas_cfg", NVS_READWRITE, &handle);
    if (error != ESP_OK)
        return error;
    error = nvs_set_u64(handle, "nguong_v1", DINH_DANG | (uint32_t)threshold);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}
