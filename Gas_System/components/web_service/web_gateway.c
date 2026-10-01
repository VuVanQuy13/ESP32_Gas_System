#include "web_gateway.h"
#include "cJSON.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool so_nguyen(const cJSON *j, int nho, int lon, int *ra)
{
    if (!cJSON_IsNumber(j) || !isfinite(j->valuedouble) || j->valuedouble < nho ||
        j->valuedouble > lon || floor(j->valuedouble) != j->valuedouble)
        return false;
    *ra = (int)j->valuedouble;
    return true;
}

static bool hex64(const cJSON *j, uint64_t *ra)
{
    if (!cJSON_IsString(j) || strlen(j->valuestring) != 16)
        return false;
    uint64_t v = 0;
    for (unsigned i = 0; i < 16; ++i) {
        char c = j->valuestring[i];
        unsigned n;
        if (c >= '0' && c <= '9')
            n = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f')
            n = (unsigned)(c - 'a' + 10);
        else
            return false;
        v = (v << 4) | n;
    }
    *ra = v;
    return true;
}

static const cJSON *truong(const cJSON *j, const char *ten)
{
    return cJSON_GetObjectItemCaseSensitive(j, ten);
}

static bool phan_tich(const char *chu, uint32_t *id, dk_su_kien *e, uint64_t *phien)
{
    cJSON *j = cJSON_ParseWithLengthOpts(chu, strlen(chu) + 1, NULL, true);
    bool dung = false;
    int so = 0;
    if (!cJSON_IsObject(j))
        goto xong;

    /* Từ chối trường lạ/trùng; giới hạn độ sâu thực tế bằng schema phẳng. */
    const char *ten[] = { "v", "id", "type", "boot", "control", "edit", "revision", "value" };
    unsigned mask = 0;
    for (const cJSON *p = j->child; p; p = p->next) {
        unsigned i = 0;
        while (i < 8 && strcmp(p->string, ten[i]))
            ++i;
        if (i == 8 || (mask & (1U << i)))
            goto xong;
        mask |= 1U << i;
    }

    if (!so_nguyen(truong(j, "v"), 1, 1, &so) || !so_nguyen(truong(j, "id"), 1, 2147483647, &so))
        goto xong;
    *id = (uint32_t)so;
    if (!hex64(truong(j, "boot"), &e->ma_khoi_dong) || !e->ma_khoi_dong)
        goto xong;
    const cJSON *t = truong(j, "type");
    if (!cJSON_IsString(t))
        goto xong;
    unsigned can = 15;
    if (!strcmp(t->valuestring, "SET_MODE") || !strcmp(t->valuestring, "SET_RELAY")) {
        e->loai = !strcmp(t->valuestring, "SET_MODE") ? DK_DAT_CHE_DO : DK_DAT_RELAY;
        if (!so_nguyen(truong(j, "value"), 0, 1, &e->gia_tri) ||
            !hex64(truong(j, "control"), phien))
            goto xong;
        can |= (1U << 4) | (1U << 7);
    } else if (!strcmp(t->valuestring, "BEGIN_EDIT"))
        e->loai = DK_BAT_DAU_CHINH;
    else if (!strcmp(t->valuestring, "SET_DRAFT") || !strcmp(t->valuestring, "SAVE_DRAFT")) {
        e->loai = !strcmp(t->valuestring, "SET_DRAFT") ? DK_DAT_BAN_NHAP : DK_YEU_CAU_LUU;
        if (!hex64(truong(j, "edit"), &e->ma_phien_chinh) ||
            !hex64(truong(j, "revision"), &e->phien_ban_mong_doi))
            goto xong;
        can |= (1U << 5) | (1U << 6);
        if (e->loai == DK_DAT_BAN_NHAP) {
            if (!so_nguyen(truong(j, "value"), 1, 4095, &e->gia_tri))
                goto xong;
            can |= 1U << 7;
        }
    } else
        goto xong;
    dung = mask == can;
xong:
    cJSON_Delete(j);
    return dung;
}

static void in_hex(uint64_t so, char ra[17])
{
    const char chu[] = "0123456789abcdef";
    for (unsigned i = 0; i < 16; ++i) {
        ra[15 - i] = chu[so & 15];
        so >>= 4;
    }
    ra[16] = '\0';
}

static esp_err_t ket_qua(char *ra, size_t n, uint32_t id, const char *tt, int ma, uint64_t lan)
{
    char hex[17];
    in_hex(lan, hex);
    int dem = snprintf(ra,
        n,
        "{\"type\":\"result\",\"id\":%" PRIu32 ",\"status\":\"%s\",\"code\":%d,\"save_id\":\"%s\"}",
        id,
        tt,
        ma,
        hex);
    return dem >= 0 && (size_t)dem < n ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t web_nhan_lenh(web_cong *g, unsigned k, const char *chu, char *ra, size_t n)
{
    if (!g || !g->dich_vu || k >= WEB_SO_KHACH || !chu || !ra || n < 192)
        return ESP_ERR_INVALID_ARG;
    dk_su_kien e = { 0 };
    uint64_t phien = 0;
    uint32_t id = 0;
    unsigned mo = 0, dong = 0;
    for (const char *p = chu; *p; ++p) {
        if (*p == '{')
            ++mo;
        if (*p == '}')
            ++dong;
    }

    /* Không chấp nhận escape trong giao thức phẳng để tránh alias khóa/chuỗi NUL. */
    if (strlen(chu) > WEB_LENH_TOI_DA || mo != 1 || dong != 1 || strchr(chu, '[') ||
        strchr(chu, ']') || strchr(chu, '\\') || !phan_tich(chu, &id, &e, &phien))
        return ket_qua(ra, n, id, "INVALID", DK_KHONG_HOP_LE, 0);
    web_phien *p = &g->khach[k];
    if (id == p->id) {
        if (strcmp(chu, p->lenh))
            return ket_qua(ra, n, id, "ID_CONFLICT", DK_DU_LIEU_CU, 0);
        memcpy(ra, p->tra_loi, strlen(p->tra_loi) + 1);
        return ESP_OK;
    }

    if (id < p->id)
        return ket_qua(ra, n, id, "OLD_ID", DK_DU_LIEU_CU, 0);
    if (p->dang_cho)
        return ket_qua(ra, n, id, "BUSY", DK_DANG_BAN, 0);
    if (g->ma_noi_bo == UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    p->id = id;
    memcpy(p->lenh, chu, strlen(chu) + 1);
    esp_err_t loi = dk_gui_lenh_web(g->dich_vu, ++g->ma_noi_bo, &e, phien);
    p->dang_cho = loi == ESP_OK ? g->ma_noi_bo : 0;
    esp_err_t tao =
        ket_qua(p->tra_loi, sizeof(p->tra_loi), id, loi == ESP_OK ? "QUEUED" : "BUSY", loi, 0);
    if (tao != ESP_OK)
        return tao;
    memcpy(ra, p->tra_loi, strlen(p->tra_loi) + 1);
    return ESP_OK;
}

esp_err_t web_don_phan_hoi(web_cong *g)
{
    if (!g || !g->dich_vu)
        return ESP_ERR_INVALID_ARG;
    for (unsigned i = 0; i < 16; ++i) {
        dk_phan_hoi p;
        esp_err_t loi = dk_doc_phan_hoi(g->dich_vu, &p);
        if (loi == ESP_ERR_NOT_FOUND)
            return ESP_OK;
        if (loi != ESP_OK)
            return loi;
        for (unsigned k = 0; k < WEB_SO_KHACH; ++k)
            if (g->khach[k].dang_cho == p.ma_lenh) {
                web_phien *s = &g->khach[k];
                s->dang_cho = 0;
                const char *tt = p.ket_qua.ma_ket_qua != DK_THANH_CONG ? "REJECTED"
                                 : p.loi_dau_ra != ESP_OK              ? "OUTPUT_ERROR"
                                 : p.ket_qua.can_luu                   ? "PENDING"
                                                                       : "APPLIED";
                loi = ket_qua(s->tra_loi,
                    sizeof(s->tra_loi),
                    s->id,
                    tt,
                    p.ket_qua.ma_ket_qua,
                    p.ket_qua.ma_lan_luu);
                if (loi != ESP_OK)
                    return loi;
            }
    }

    return ESP_OK;
}

esp_err_t web_tao_ban_tin(const dk_ban_chup *b, uint64_t now, char *ra, size_t n)
{
    if (!b || !ra || !n || !b->trang_thai.da_khoi_tao)
        return ESP_ERR_INVALID_ARG;
    const dk_trang_thai *s = &b->trang_thai;
    bool cu = now < s->hien_tai_ms || now - s->hien_tai_ms >= 1000;
    uint64_t tuoi = s->da_co_mau && now >= s->luc_lay_mau_ms ? now - s->luc_lay_mau_ms : UINT64_MAX;
    char gia_tri[16] = "null";
    if (!cu && s->tinh_trang_cam_bien == DK_SAN_SANG && tuoi < s->cau_hinh.han_du_lieu_ms)
        snprintf(gia_tri, sizeof(gia_tri), "%d", s->mau);
    char hex[6][17];
    in_hex(s->ma_khoi_dong, hex[0]);
    in_hex(b->phien_ban, hex[1]);
    in_hex(b->phien_dieu_khien, hex[2]);
    in_hex(s->ma_phien_chinh, hex[3]);
    in_hex(s->phien_ban_nhap, hex[4]);
    in_hex(s->ma_lan_luu, hex[5]);
    int dem = snprintf(ra,
        n,
        "{\"type\":\"state\",\"v\":1,\"boot\":\"%s\",\"seq\":\"%s\",\"control\":\"%"
        "s\",\"edit\":\"%s\",\"revision\":\"%s\",\"save_id\":\"%s\",\"mode\":%d,"
        "\"sensor\":%d,\"relay\":%d,\"relay_error\":%d,\"save_error\":%d,"
        "\"threshold\":%d,\"draft\":%d,\"editing\":%d,\"unknown\":%d,\"config\":%d,"
        "\"value\":%s,\"stale\":%d,"
        "\"minimum\":%d,\"maximum\":%d,\"step\":10}",
        hex[0],
        hex[1],
        hex[2],
        hex[3],
        hex[4],
        hex[5],
        s->che_do,
        s->tinh_trang_cam_bien,
        s->relay_bat,
        b->loi_dau_ra,
        b->loi_luu,
        s->nguong,
        s->ban_nhap,
        s->trang_thai_chinh,
        s->chua_ro_ket_qua_luu,
        s->cau_hinh_hop_le,
        gia_tri,
        cu,
        s->cau_hinh.nguong_nho_nhat,
        s->cau_hinh.nguong_lon_nhat);
    return dem >= 0 && (size_t)dem < n ? ESP_OK : ESP_ERR_NO_MEM;
}
