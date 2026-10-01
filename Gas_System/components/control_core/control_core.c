#include "control_core.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool nguong_hop_le(const dk_cau_hinh *tham_so, int gia_tri)
{
    return gia_tri >= tham_so->nguong_nho_nhat && gia_tri <= tham_so->nguong_lon_nhat;
}

dk_ma_ket_qua dk_khoi_tao(dk_trang_thai *trang_thai,
    const dk_cau_hinh *tham_so,
    uint64_t ma_khoi_dong,
    uint64_t hien_tai,
    bool du_lieu_luu_hop_le,
    int nguong)
{
    if (trang_thai == NULL || tham_so == NULL || ma_khoi_dong == 0 ||
        tham_so->thoi_gian_on_dinh_ms == 0 || tham_so->han_du_lieu_ms == 0 ||
        tham_so->han_chinh_ms == 0 || tham_so->han_luu_ms == 0 || tham_so->so_mau_can_thiet == 0 ||
        tham_so->nguong_nho_nhat < 1 || tham_so->nguong_lon_nhat > 4095 ||
        tham_so->nguong_lon_nhat < tham_so->nguong_nho_nhat || tham_so->do_tre_nguong <= 0 ||
        tham_so->do_tre_nguong > tham_so->nguong_nho_nhat ||
        !nguong_hop_le(tham_so, tham_so->nguong_nhap_ban_dau)) {
        return DK_KHONG_HOP_LE;
    }
    dk_cau_hinh ban_sao = *tham_so;
    memset(trang_thai, 0, sizeof(*trang_thai));
    trang_thai->cau_hinh = ban_sao;
    trang_thai->da_khoi_tao = true;
    trang_thai->ma_khoi_dong = ma_khoi_dong;
    trang_thai->luc_khoi_dong_ms = hien_tai;
    trang_thai->hien_tai_ms = hien_tai;
    trang_thai->che_do = DK_TU_DONG;
    trang_thai->tinh_trang_cam_bien = DK_DANG_ON_DINH;
    trang_thai->trang_thai_chinh = DK_KHONG_CHINH;
    trang_thai->cau_hinh_hop_le = du_lieu_luu_hop_le && nguong_hop_le(&ban_sao, nguong);
    trang_thai->nguong = trang_thai->cau_hinh_hop_le ? nguong : 0;
    return DK_THANH_CONG;
}

static void danh_gia_tu_dong(dk_trang_thai *trang_thai, bool vua_chuyen_vao)
{
    if (trang_thai->che_do != DK_TU_DONG) {
        return;
    }

    if (vua_chuyen_vao || trang_thai->tinh_trang_cam_bien != DK_SAN_SANG ||
        !trang_thai->cau_hinh_hop_le) {
        trang_thai->relay_bat = false;
    }

    if (trang_thai->tinh_trang_cam_bien == DK_SAN_SANG && trang_thai->cau_hinh_hop_le) {
        if (trang_thai->mau >= trang_thai->nguong) {
            trang_thai->relay_bat = true;
        } else if (trang_thai->mau <= trang_thai->nguong - trang_thai->cau_hinh.do_tre_nguong) {
            trang_thai->relay_bat = false;
        }
    }
}

static void bao_loi_cam_bien(dk_trang_thai *trang_thai)
{
    trang_thai->tinh_trang_cam_bien = DK_LOI_CAM_BIEN;
    trang_thai->so_mau_lien_tiep = 0;
    danh_gia_tu_dong(trang_thai, false);
}

static void kiem_tra_cac_han(dk_trang_thai *trang_thai)
{
    uint64_t moc_cuoi =
        trang_thai->da_co_mau ? trang_thai->luc_lay_mau_ms : trang_thai->luc_khoi_dong_ms;
    if (trang_thai->hien_tai_ms - moc_cuoi >= trang_thai->cau_hinh.han_du_lieu_ms) {
        bao_loi_cam_bien(trang_thai);
    }

    if (trang_thai->trang_thai_chinh == DK_DANG_CHINH &&
        trang_thai->hien_tai_ms - trang_thai->luc_thao_tac_chinh_ms >=
            trang_thai->cau_hinh.han_chinh_ms) {
        trang_thai->trang_thai_chinh = DK_KHONG_CHINH;
        trang_thai->ban_nhap = 0;
    }

    if (trang_thai->trang_thai_chinh == DK_DANG_LUU &&
        trang_thai->hien_tai_ms - trang_thai->luc_bat_dau_luu_ms >=
            trang_thai->cau_hinh.han_luu_ms) {
        /* Có thể đã ghi thành công: giữ giao dịch này chờ xác minh. */
        trang_thai->chua_ro_ket_qua_luu = true;
    }
}

static dk_ma_ket_qua tiep_nhan_mau(dk_trang_thai *trang_thai, const dk_su_kien *su_kien)
{
    if (su_kien->so_thu_tu == 0 || su_kien->so_thu_tu <= trang_thai->thu_tu_mau ||
        su_kien->luc_lay_mau_ms < trang_thai->luc_khoi_dong_ms ||
        su_kien->luc_lay_mau_ms > trang_thai->hien_tai_ms ||
        (trang_thai->thu_tu_mau != 0 && su_kien->luc_lay_mau_ms <= trang_thai->luc_lay_mau_ms)) {
        return DK_DU_LIEU_CU;
    }
    bool bi_mat_mau =
        trang_thai->thu_tu_mau != 0 && su_kien->so_thu_tu - trang_thai->thu_tu_mau != 1;
    trang_thai->thu_tu_mau = su_kien->so_thu_tu;
    trang_thai->luc_lay_mau_ms = su_kien->luc_lay_mau_ms;
    trang_thai->da_co_mau = true;
    if (!su_kien->hop_le || su_kien->gia_tri < 0 || su_kien->gia_tri > 4095 ||
        trang_thai->hien_tai_ms - su_kien->luc_lay_mau_ms >= trang_thai->cau_hinh.han_du_lieu_ms) {
        bao_loi_cam_bien(trang_thai);
        return DK_KHONG_HOP_LE;
    }

    if (bi_mat_mau) {
        bao_loi_cam_bien(trang_thai);
    }
    bool truoc_do_san_sang = trang_thai->tinh_trang_cam_bien == DK_SAN_SANG;
    trang_thai->mau = su_kien->gia_tri;
    if (trang_thai->so_mau_lien_tiep < trang_thai->cau_hinh.so_mau_can_thiet) {
        ++trang_thai->so_mau_lien_tiep;
    }

    if (trang_thai->hien_tai_ms - trang_thai->luc_khoi_dong_ms >=
            trang_thai->cau_hinh.thoi_gian_on_dinh_ms &&
        trang_thai->so_mau_lien_tiep >= trang_thai->cau_hinh.so_mau_can_thiet) {
        trang_thai->tinh_trang_cam_bien = DK_SAN_SANG;
    }
    danh_gia_tu_dong(
        trang_thai, !truoc_do_san_sang && trang_thai->tinh_trang_cam_bien == DK_SAN_SANG);
    return DK_THANH_CONG;
}

static dk_ma_ket_qua kiem_tra_phien_chinh(
    const dk_trang_thai *trang_thai, const dk_su_kien *su_kien)
{
    if (trang_thai->trang_thai_chinh == DK_DANG_LUU) {
        return DK_DANG_BAN;
    }

    if (trang_thai->trang_thai_chinh != DK_DANG_CHINH) {
        return DK_KHONG_CO_PHIEN_CHINH;
    }

    if (su_kien->ma_phien_chinh != trang_thai->ma_phien_chinh ||
        su_kien->phien_ban_mong_doi != trang_thai->phien_ban_nhap) {
        return DK_DU_LIEU_CU;
    }

    return DK_THANH_CONG;
}

static dk_ma_ket_qua ap_dung_su_kien(
    dk_trang_thai *trang_thai, const dk_su_kien *su_kien, dk_ket_qua *ket_qua)
{
    dk_ma_ket_qua ma_ket_qua;
    if (su_kien->ma_khoi_dong != trang_thai->ma_khoi_dong) {
        return DK_DU_LIEU_CU;
    }
    switch (su_kien->loai) {
    case DK_KIEM_TRA_THOI_GIAN:
        return DK_THANH_CONG;
    case DK_MAU_MOI:
        return tiep_nhan_mau(trang_thai, su_kien);
    case DK_DAT_CHE_DO:
        if (su_kien->gia_tri != DK_TU_DONG && su_kien->gia_tri != DK_THU_CONG)
            return DK_KHONG_HOP_LE;
        if (trang_thai->trang_thai_chinh != DK_KHONG_CHINH)
            return DK_DANG_BAN;
        if (trang_thai->che_do != (dk_che_do)su_kien->gia_tri) {
            trang_thai->che_do = (dk_che_do)su_kien->gia_tri;
            danh_gia_tu_dong(trang_thai, true);
        }

        return DK_THANH_CONG;
    case DK_DAT_RELAY:
        if (su_kien->gia_tri != 0 && su_kien->gia_tri != 1)
            return DK_KHONG_HOP_LE;
        if (trang_thai->trang_thai_chinh != DK_KHONG_CHINH)
            return DK_DANG_BAN;
        if (trang_thai->che_do != DK_THU_CONG)
            return DK_SAI_CHE_DO;
        trang_thai->relay_bat = su_kien->gia_tri != 0;
        return DK_THANH_CONG;
    case DK_BAT_DAU_CHINH:
        if (trang_thai->trang_thai_chinh != DK_KHONG_CHINH)
            return DK_DANG_BAN;
        if (trang_thai->ma_phien_chinh == UINT64_MAX)
            return DK_HET_MA_DINH_DANH;
        ++trang_thai->ma_phien_chinh;
        trang_thai->phien_ban_nhap = 1;
        trang_thai->ban_nhap = trang_thai->cau_hinh_hop_le
                                   ? trang_thai->nguong
                                   : trang_thai->cau_hinh.nguong_nhap_ban_dau;
        trang_thai->luc_thao_tac_chinh_ms = trang_thai->hien_tai_ms;
        trang_thai->trang_thai_chinh = DK_DANG_CHINH;
        return DK_THANH_CONG;
    case DK_DAT_BAN_NHAP:
        ma_ket_qua = kiem_tra_phien_chinh(trang_thai, su_kien);
        if (ma_ket_qua != DK_THANH_CONG)
            return ma_ket_qua;
        if (!nguong_hop_le(&trang_thai->cau_hinh, su_kien->gia_tri))
            return DK_KHONG_HOP_LE;
        if (su_kien->gia_tri != trang_thai->ban_nhap) {
            if (trang_thai->phien_ban_nhap == UINT64_MAX)
                return DK_HET_MA_DINH_DANH;
            trang_thai->ban_nhap = su_kien->gia_tri;
            ++trang_thai->phien_ban_nhap;
        }
        trang_thai->luc_thao_tac_chinh_ms = trang_thai->hien_tai_ms;
        return DK_THANH_CONG;
    case DK_YEU_CAU_LUU:
        ma_ket_qua = kiem_tra_phien_chinh(trang_thai, su_kien);
        if (ma_ket_qua != DK_THANH_CONG)
            return ma_ket_qua;
        if (trang_thai->ma_lan_luu == UINT64_MAX)
            return DK_HET_MA_DINH_DANH;
        ++trang_thai->ma_lan_luu;
        trang_thai->trang_thai_chinh = DK_DANG_LUU;
        trang_thai->luc_bat_dau_luu_ms = trang_thai->hien_tai_ms;
        trang_thai->chua_ro_ket_qua_luu = false;
        ket_qua->can_luu = true;
        ket_qua->ma_lan_luu = trang_thai->ma_lan_luu;
        ket_qua->nguong_can_luu = trang_thai->ban_nhap;
        return DK_THANH_CONG;
    case DK_LUU_THANH_CONG:
    case DK_LUU_THAT_BAI:
        if (trang_thai->trang_thai_chinh != DK_DANG_LUU ||
            su_kien->ma_lan_luu != trang_thai->ma_lan_luu)
            return DK_DU_LIEU_CU;
        trang_thai->chua_ro_ket_qua_luu = false;
        if (su_kien->loai == DK_LUU_THAT_BAI) {
            trang_thai->trang_thai_chinh = DK_DANG_CHINH;
            trang_thai->luc_thao_tac_chinh_ms = trang_thai->hien_tai_ms;
            return DK_LOI_LUU_TRU;
        }
        trang_thai->nguong = trang_thai->ban_nhap;
        trang_thai->cau_hinh_hop_le = true;
        trang_thai->trang_thai_chinh = DK_KHONG_CHINH;
        trang_thai->ban_nhap = 0;
        danh_gia_tu_dong(trang_thai, false);
        return DK_THANH_CONG;
    default:
        return DK_KHONG_HOP_LE;
    }
}

dk_ma_ket_qua dk_xu_ly_su_kien(
    dk_trang_thai *trang_thai, const dk_su_kien *su_kien, uint64_t hien_tai, dk_ket_qua *ket_qua)
{
    if (ket_qua == NULL)
        return DK_KHONG_HOP_LE;
    memset(ket_qua, 0, sizeof(*ket_qua));
    ket_qua->ma_ket_qua = DK_KHONG_HOP_LE;
    if (trang_thai == NULL || su_kien == NULL || !trang_thai->da_khoi_tao)
        return DK_KHONG_HOP_LE;
    ket_qua->su_kien = su_kien->loai;
    ket_qua->thoi_diem_ms = hien_tai;
    bool relay_truoc_do = trang_thai->relay_bat;
    if (hien_tai < trang_thai->hien_tai_ms) {
        ket_qua->ma_ket_qua = DK_LOI_THOI_GIAN;
    } else {
        trang_thai->hien_tai_ms = hien_tai;

        /* Lệnh bị từ chối vẫn phải cho phép phát hiện dữ liệu cảm biến hết hạn. */
        kiem_tra_cac_han(trang_thai);
        ket_qua->ma_ket_qua = ap_dung_su_kien(trang_thai, su_kien, ket_qua);
    }
    ket_qua->relay_thay_doi = relay_truoc_do != trang_thai->relay_bat;
    ket_qua->relay_bat = trang_thai->relay_bat;
    ket_qua->che_do = trang_thai->che_do;
    ket_qua->tinh_trang_cam_bien = trang_thai->tinh_trang_cam_bien;
    ket_qua->trang_thai_chinh = trang_thai->trang_thai_chinh;
    ket_qua->chua_ro_ket_qua_luu = trang_thai->chua_ro_ket_qua_luu;
    return ket_qua->ma_ket_qua;
}

static uint64_t tinh_han(uint64_t bat_dau, uint64_t khoang_thoi_gian)
{
    return khoang_thoi_gian > UINT64_MAX - bat_dau ? UINT64_MAX : bat_dau + khoang_thoi_gian;
}

uint64_t dk_han_ke_tiep(const dk_trang_thai *trang_thai)
{
    if (trang_thai == NULL || !trang_thai->da_khoi_tao)
        return UINT64_MAX;
    uint64_t han_tiep = UINT64_MAX;
    if (trang_thai->tinh_trang_cam_bien != DK_LOI_CAM_BIEN || trang_thai->so_mau_lien_tiep > 0) {
        han_tiep = tinh_han(
            trang_thai->da_co_mau ? trang_thai->luc_lay_mau_ms : trang_thai->luc_khoi_dong_ms,
            trang_thai->cau_hinh.han_du_lieu_ms);
    }
    uint64_t han_chinh_luu = UINT64_MAX;
    if (trang_thai->trang_thai_chinh == DK_DANG_CHINH)
        han_chinh_luu =
            tinh_han(trang_thai->luc_thao_tac_chinh_ms, trang_thai->cau_hinh.han_chinh_ms);
    if (trang_thai->trang_thai_chinh == DK_DANG_LUU && !trang_thai->chua_ro_ket_qua_luu)
        han_chinh_luu = tinh_han(trang_thai->luc_bat_dau_luu_ms, trang_thai->cau_hinh.han_luu_ms);
    return han_chinh_luu < han_tiep ? han_chinh_luu : han_tiep;
}
