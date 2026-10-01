#ifndef TAC_VU_DIEU_KHIEN_H
#define TAC_VU_DIEU_KHIEN_H

#include "control_core.h"
#include "esp_err.h"

typedef struct dk_dich_vu dk_dich_vu;

typedef enum {
    DK_NUT_BAT_TANG,
    DK_NUT_TAT_GIAM,
    DK_NUT_CHE_DO,
    DK_NUT_CHINH_LUU
} dk_nut;

/* Nút là thao tác tương đối; owner diễn giải theo trạng thái lúc xử lý.
 * Kết quả xuất qua log/snapshot, không chiếm hàng phản hồi của web. */
esp_err_t dk_gui_nut(dk_dich_vu *dich_vu, dk_nut nut);

typedef struct {
    uint64_t phien_ban;
    uint64_t phien_dieu_khien; /* Chỉ đổi khi mode hoặc relay đổi, không theo telemetry. */
    dk_trang_thai trang_thai;
    esp_err_t loi_dau_ra; /* GPIO có thể lỗi dù lõi đã chấp nhận ý định. */
    bool loi_luu;         /* Giữ lỗi Save để UI không mất thông báo khi có mẫu cảm biến mới. */
} dk_ban_chup;

typedef struct {
    uint64_t ma_lenh;
    dk_ket_qua ket_qua;
    esp_err_t loi_dau_ra;
} dk_phan_hoi;

typedef struct {
    uint64_t ma_khoi_dong, ma_lan_luu;
    int nguong;
} dk_viec_luu;

typedef struct {
    dk_cau_hinh loi;
    uint64_t ma_khoi_dong;
    bool du_lieu_luu_hop_le;
    int nguong_da_luu;

    /* Callback phải ngắn, không chờ mạng/LCD/flash. Không gọi lại API dịch vụ.
     * Được gọi OFF lúc khởi tạo, sau đó chỉ từ control_task.
     * ngu_canh phải tồn tại suốt vòng đời dịch vụ, không trỏ vào biến tạm. */
    esp_err_t (*xuat_relay)(bool bat, void *ngu_canh);
    void *ngu_canh;
} dk_thiet_lap_tac_vu;

/* Gọi một lần trong startup trước khi công bố con trỏ cho các task khác.
 * Dịch vụ sống đến reset, không hỗ trợ xóa nóng khi producer còn hoạt động. */
esp_err_t dk_bat_dau(dk_dich_vu **dich_vu, const dk_thiet_lap_tac_vu *thiet_lap);

/* API dành cho task, không dùng trong ISR. ESP_OK chỉ nghĩa đã xếp hàng.
 * ma_lenh khác 0 cho lệnh; sensor dùng dk_gui_mau, không tạo phản hồi lệnh.
 * Bên định tuyến cấp ma_lenh duy nhất, đọc phản hồi và chuyển đúng client. */
esp_err_t dk_gui_lenh(dk_dich_vu *dich_vu, uint64_t ma_lenh, const dk_su_kien *su_kien);

/* Gateway duy nhất gọi; owner kiểm tra tuổi queue <1000 ms và revision mode/relay. */
esp_err_t dk_gui_lenh_web(
    dk_dich_vu *dich_vu, uint64_t ma_lenh, const dk_su_kien *su_kien, uint64_t phien_dieu_khien);
esp_err_t dk_gui_mau(dk_dich_vu *dich_vu, const dk_su_kien *su_kien);
esp_err_t dk_doc_phan_hoi(dk_dich_vu *dich_vu, dk_phan_hoi *phan_hoi);

/* Hai mailbox độc lập, đọc không lấy mất bản chụp của giao diện còn lại. */
esp_err_t dk_doc_lcd(dk_dich_vu *dich_vu, dk_ban_chup *ban_chup);
esp_err_t dk_doc_web(dk_dich_vu *dich_vu, dk_ban_chup *ban_chup);
esp_err_t dk_nhan_viec_luu(dk_dich_vu *dich_vu, dk_viec_luu *viec);

/* Chỉ storage_task gọi; gateway không được giả mạo kết quả lưu. */
esp_err_t dk_bao_ket_qua_luu(dk_dich_vu *dich_vu, const dk_su_kien *su_kien);

#endif
