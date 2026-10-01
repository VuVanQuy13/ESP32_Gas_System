idf_component_register(SRCS "network.c" INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event esp_timer nvs_flash)

# Profile bàn giao tạo header riêng trong build, không sửa source chứa thông tin cá nhân.
if(DEFINED GAS_WIFI_CONFIG_DIR)
    if(NOT EXISTS "${GAS_WIFI_CONFIG_DIR}/wifi_local.h")
        message(FATAL_ERROR "Thieu wifi_local.h trong GAS_WIFI_CONFIG_DIR")
    endif()
    target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE "${GAS_WIFI_CONFIG_DIR}")
endif()
