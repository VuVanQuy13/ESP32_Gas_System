#ifndef LOI_DIEU_KHIEN_H
#define LOI_DIEU_KHIEN_H

#include <stdbool.h>
#include <stdint.h>

/* C thuần: bên gọi cung cấp thời gian mili giây tăng đơn điệu và sở hữu trạng thái.
 * Chỉ gọi từ một tác vụ. Kiểm tra giao thức/chống lệnh lặp nằm ngoài lõi. */
typedef enum {
    DK_TU_DONG,
    DK_THU_CONG
} dk_che_do;

typedef enum {
    DK_DANG_ON_DINH,
    DK_SAN_SANG,
    DK_LOI_CAM_BIEN
} dk_tinh_trang_cam_bien;

typedef enum {
    DK_KHONG_CHINH,
    DK_DANG_CHINH,
    DK_DANG_LUU
} dk_trang_thai_chinh;

typedef enum {
    DK_KIEM_TRA_THOI_GIAN,
    DK_MAU_MOI,
    DK_DAT_CHE_DO,
    DK_DAT_RELAY,
    DK_BAT_DAU_CHINH,
    DK_DAT_BAN_NHAP,
    DK_YEU_CAU_LUU,
    DK_LUU_THANH_CONG,
    DK_LUU_THAT_BAI
} dk_loai_su_kien;

typedef enum {
    DK_THANH_CONG,
    DK_KHONG_HOP_LE,
    DK_LOI_THOI_GIAN,
    DK_SAI_CHE_DO,
    DK_DANG_BAN,
    DK_DU_LIEU_CU,
    DK_KHONG_CO_PHIEN_CHINH,
    DK_LOI_LUU_TRU,
    DK_HET_MA_DINH_DANH
} dk_ma_ket_qua;

typedef struct {
    uint64_t thoi_gian_on_dinh_ms, han_du_lieu_ms, han_chinh_ms, han_luu_ms;
    uint32_t so_mau_can_thiet;
    int nguong_nho_nhat, nguong_lon_nhat, do_tre_nguong, nguong_nhap_ban_dau;
} dk_cau_hinh;

typedef struct {
    dk_loai_su_kien loai;
    uint64_t ma_khoi_dong;              /* Sự kiện phải thuộc lần khởi động này. */
    uint64_t so_thu_tu, luc_lay_mau_ms; /* Thời điểm lấy mẫu, không phải lúc nhận. */
    bool hop_le;                        /* Đọc và kiểm tra chất lượng mẫu thành công. */
    int gia_tri;                        /* Mẫu thô, bản nháp, chế độ hoặc relay 0/1. */
    uint64_t ma_phien_chinh, phien_ban_mong_doi, ma_lan_luu;
} dk_su_kien;

typedef struct {
    bool da_khoi_tao;
    dk_cau_hinh cau_hinh;
    uint64_t ma_khoi_dong, luc_khoi_dong_ms, hien_tai_ms;
    dk_che_do che_do;
    dk_tinh_trang_cam_bien tinh_trang_cam_bien;
    dk_trang_thai_chinh trang_thai_chinh;
    bool relay_bat, cau_hinh_hop_le, da_co_mau;
    int nguong, mau, ban_nhap;
    uint32_t so_mau_lien_tiep;
    uint64_t thu_tu_mau, luc_lay_mau_ms;
    uint64_t ma_phien_chinh, phien_ban_nhap, luc_thao_tac_chinh_ms;
    uint64_t ma_lan_luu, luc_bat_dau_luu_ms;
    bool chua_ro_ket_qua_luu;
} dk_trang_thai;

/* Mỗi lần xử lý trả bản ghi có cấu trúc, kể cả khi từ chối lệnh.
 * Tác vụ điều phối phải ghi nhật ký và gửi yêu cầu lưu đúng một lần. */
typedef struct {
    dk_ma_ket_qua ma_ket_qua;
    uint64_t thoi_diem_ms;
    dk_loai_su_kien su_kien;
    bool relay_thay_doi, relay_bat;
    bool can_luu;
    int nguong_can_luu;
    uint64_t ma_lan_luu;
    dk_che_do che_do;
    dk_tinh_trang_cam_bien tinh_trang_cam_bien;
    dk_trang_thai_chinh trang_thai_chinh;
    bool chua_ro_ket_qua_luu;
} dk_ket_qua;

dk_ma_ket_qua dk_khoi_tao(dk_trang_thai *trang_thai,
    const dk_cau_hinh *cau_hinh,
    uint64_t ma_khoi_dong,
    uint64_t hien_tai_ms,
    bool du_lieu_luu_hop_le,
    int nguong_da_luu);
dk_ma_ket_qua dk_xu_ly_su_kien(dk_trang_thai *trang_thai,
    const dk_su_kien *su_kien,
    uint64_t hien_tai_ms,
    dk_ket_qua *ket_qua);

/* UINT64_MAX: không có hạn chờ hoặc hạn vượt miền biểu diễn. */
uint64_t dk_han_ke_tiep(const dk_trang_thai *trang_thai);

#endif
