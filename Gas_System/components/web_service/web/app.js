"use strict";

// Các hàm thuần dùng chung với kiểm thử Node, không cần board hoặc DOM.
function taoLenh(s, loai, giaTri, id)
{
    if (!s || s.stale || !Number.isInteger(id) || id < 1 || id > 2147483647)
        throw Error("Trạng thái chưa sẵn sàng");
    const e = { v : 1, id, type : loai, boot : s.boot };
    if (loai === "SET_MODE" || loai === "SET_RELAY") {
        e.control = s.control;
        e.value = giaTri;
    } else if (loai === "SET_DRAFT" || loai === "SAVE_DRAFT") {
        e.edit = s.edit;
        e.revision = s.revision;
        if (loai === "SET_DRAFT")
            e.value = giaTri;
    } else if (loai !== "BEGIN_EDIT")
        throw Error("Loại lệnh không hợp lệ");
    return e;
}

function trangThaiHopLe(s)
{
    const hex = x => typeof x === "string" && /^[0-9a-f]{16}$/.test(x);
    return s && s.type === "state" && s.v === 2 &&
           [ s.boot, s.seq, s.control, s.edit, s.revision, s.save_id ].every(hex) &&
           [ s.mode, s.config, s.relay, s.save_error, s.stale, s.unknown,
             s.fire_valid, s.fire, s.fire_confirming ].every(
               x => x === 0 || x === 1) &&
           [ 0, 1, 2 ].includes(s.sensor) && [ 0, 1, 2 ].includes(s.editing) &&
           Number.isInteger(s.relay_error) &&
           (s.value === null || Number.isInteger(s.value) && s.value >= 0 && s.value <= 4095) &&
           Number.isInteger(s.minimum) && Number.isInteger(s.maximum) && s.minimum >= 1 &&
           s.maximum <= 4095 && s.minimum <= s.maximum && Number.isInteger(s.threshold) &&
           Number.isInteger(s.draft) && s.step === 10;
}

if (typeof module !== "undefined")
    module.exports = { taoLenh, trangThaiHopLe };
if (typeof document !== "undefined")
    (() => {
        const el = id => document.getElementById(id);
        let ws = null, s = null, id = 0, cho = null, lucNhan = 0, lucKetNoi = 0, keo = null,
            noiLai = null, lanLuuCho = null;
        const thongBao = chu => { el("thong-bao").textContent = chu; };
        function ve()
        {
            const online =
                ws && ws.readyState === 1 && s && Date.now() - lucNhan < 2000 && !s.stale;
            el("ket-noi").textContent = online ? "Đã đồng bộ" : "Chưa đồng bộ";
            el("gia-tri").textContent = online && s.value !== null ? s.value : "—";
            const vuot = online && s.config && s.value !== null && s.value >= s.threshold;
            el("gia-tri").closest("section").classList.toggle("bao-dong", !!vuot);
            el("cam-bien").textContent = !online ? "Dữ liệu chưa cập nhật"
                                         : s.sensor === 2
                                             ? "Lỗi cảm biến — không kết luận khí an toàn"
                                         : s.sensor === 0   ? "Đang chờ cảm biến ổn định"
                                         : s.value === null ? "Mẫu không còn hợp lệ"
                                         : vuot             ? "Đang vượt ngưỡng"
                                                            : "Mẫu hợp lệ, chưa vượt ngưỡng";
            el("che-do").textContent = s ? (s.mode === 0 ? "Auto" : "Manual") : "—";
            el("relay").textContent = !online || s.relay_error ? "Chưa rõ"
                                      : s.relay                ? "Bật"
                                                               : "Tắt";
            el("nguong").textContent = s && s.config ? s.threshold : "Chưa lưu";
            const fire_card = el("trang-thai-lua");
            fire_card.classList.remove("bao-dong-lua", "bao-xac-nhan-lua", "bao-loi-lua");
            if (!online) {
                el("lua").textContent = "Chưa đồng bộ";
            } else if (!s.fire_valid) {
                el("lua").textContent = "Không đọc được D0";
                fire_card.classList.add("bao-loi-lua");
            } else if (s.fire) {
                el("lua").textContent = "Phát hiện lửa";
                fire_card.classList.add("bao-dong-lua");
            } else if (s.fire_confirming) {
                el("lua").textContent = "Đang xác nhận hết";
                fire_card.classList.add("bao-xac-nhan-lua");
            } else {
                el("lua").textContent = "Không phát hiện";
            }
            const chinh = !!s && s.editing === 1, luu = !!s && s.editing === 2,
                  khoa = !online || !!cho;
            el("tang").textContent = chinh ? "Tăng +" : "Bật relay";
            el("giam").textContent = chinh ? "Giảm −" : "Tắt relay";
            for (const nut of ["tang", "giam"])
                el(nut).disabled = khoa || luu || (!chinh && s.mode !== 1) || !!keo;
            el("doi-che-do").disabled = khoa || chinh || luu || !!keo;
            el("doi-che-do").textContent = s && s.mode === 0 ? "Chuyển Manual" : "Chuyển Auto";
            el("chinh").disabled = khoa || luu || !!keo;
            el("chinh").textContent = chinh ? "Lưu ngưỡng" : luu ? "Đang lưu…" : "Chỉnh ngưỡng";
            el("vung-chinh").hidden = !s || s.editing === 0;
            el("thanh-truot").disabled = khoa || !chinh;
            if (s) {
                el("ban-nhap").textContent = s.draft;
                el("thanh-truot").min = s.minimum;
                el("thanh-truot").max = s.maximum;
                if (!keo)
                    el("thanh-truot").value = s.draft;
            }
            el("luu").textContent =
                !online                 ? ""
                : luu                   ? (s.unknown ? "Chưa rõ kết quả lưu — tiếp tục chờ ESP32"
                                                     : "Đang ghi ngưỡng vào NVS…")
                : chinh && s.save_error ? "Lưu thất bại. Bản nháp được giữ để thử lại."
                : s && lanLuuCho === s.save_id && s.editing === 0 && s.config && !s.save_error
                    ? "Đã lưu ngưỡng vào NVS."
                    : "";
            el("huong-dan").textContent =
                chinh ? "Web và nút cứng cùng chỉnh một bản nháp."
                      : "Manual: dùng Bật/Tắt. Auto: relay theo dữ liệu cảm biến.";
        }
        function gui(loai, giaTri, ban = s)
        {
            if (!ws || ws.readyState !== 1 || !s || cho || Date.now() - lucNhan >= 2000 || s.stale)
                return;
            if (++id > 2147483647) {
                ws.close();
                return;
            }
            const lenh = taoLenh(ban, loai, giaTri, id);
            cho = { id, luc : Date.now() };
            try {
                ws.send(JSON.stringify(lenh));
                thongBao("Đang chờ ESP32 xử lý…");
            } catch {
                cho = null;
                ws.close();
                thongBao("Không gửi được lệnh; không tự gửi lại.");
            }
            ve();
        }
        el("tang").onclick = () => s.editing === 1
                                       ? gui("SET_DRAFT", Math.min(s.maximum, s.draft + s.step))
                                       : gui("SET_RELAY", 1);
        el("giam").onclick = () => s.editing === 1
                                       ? gui("SET_DRAFT", Math.max(s.minimum, s.draft - s.step))
                                       : gui("SET_RELAY", 0);
        el("doi-che-do").onclick = () => gui("SET_MODE", s.mode === 0 ? 1 : 0);
        el("chinh").onclick = () => gui(s.editing === 1 ? "SAVE_DRAFT" : "BEGIN_EDIT");
        el("thanh-truot").oninput = () => {
            if (!s || cho || s.editing !== 1)
                return;
            if (!keo)
                keo = {...s };
            el("dang-keo").textContent = `Đang chọn ${el("thanh-truot").value} — thả để gửi`;
            ve();
        };
        el("thanh-truot").onchange = () => {
            const ban = keo;
            const giaTri = Number(el("thanh-truot").value);
            keo = null;
            el("dang-keo").textContent = "";
            if (ban)
                gui("SET_DRAFT", giaTri, ban);
            else
                ve();
        };
        function ketNoi()
        {
            lucKetNoi = Date.now();
            const socket = new WebSocket(
                `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws`);
            ws = socket;
            socket.onmessage = e => {
                if (ws !== socket)
                    return;
                let tin;
                try {
                    tin = JSON.parse(e.data);
                } catch {
                    socket.close();
                    return;
                }
                if (tin.type === "state") {
                    if (!trangThaiHopLe(tin)) {
                        socket.close();
                        return;
                    }
                    if (s && s.boot === tin.boot && tin.seq < s.seq)
                        return;
                    if (s && s.boot !== tin.boot) {
                        cho = null;
                        keo = null;
                        lanLuuCho = null;
                        thongBao("ESP32 vừa khởi động lại; đã bỏ thao tác cũ.");
                    }
                    if (keo && (keo.edit !== tin.edit || keo.revision !== tin.revision ||
                                   tin.editing !== 1)) {
                        keo = null;
                        el("dang-keo").textContent = "";
                        thongBao(
                            "Bản nháp đã thay đổi từ nơi khác. Hãy thao tác lại trên giá trị mới.");
                    }
                    s = tin;
                    lucNhan = Date.now();
                } else if (tin.type === "result" && cho && tin.id === cho.id &&
                           tin.status !== "QUEUED") {
                    if (tin.status === "PENDING")
                        lanLuuCho = tin.save_id;
                    cho = null;
                    thongBao(tin.status === "APPLIED" ? "ESP32 đã áp dụng lệnh."
                             : tin.status === "PENDING"
                                 ? "Đã nhận yêu cầu lưu; theo dõi kết quả ở trên."
                             : tin.status === "OUTPUT_ERROR"
                                 ? "Lệnh đã xử lý nhưng xuất GPIO lỗi."
                                 : `Lệnh bị từ chối (${tin.status}, mã ${
                                       tin.code}). Đã giữ trạng thái từ ESP32.`);
                }
                ve();
            };
            socket.onerror = () => socket.close();
            socket.onclose = () => {
                if (ws !== socket)
                    return;
                thongBao(
                    cho ? "Mất kết nối khi chờ kết quả; chưa rõ lệnh đã áp dụng hay chưa. Không tự gửi lại."
                        : "Mất kết nối. Bản nháp vẫn do ESP32 quản lý.");
                cho = null;
                keo = null;
                s = null;
                id = 0;
                lanLuuCho = null;
                el("dang-keo").textContent = "";
                ve();
                if (noiLai)
                    clearTimeout(noiLai);
                noiLai = setTimeout(ketNoi, 1500);
            };
        }
        setInterval(() => {
            if (ws && ws.readyState === 1 &&
                ((!s && Date.now() - lucKetNoi >= 2000) || (s && Date.now() - lucNhan >= 2000) ||
                    (cho && Date.now() - cho.luc >= 3000)))
                ws.close();
            ve();
        }, 250);
        ve();
        ketNoi();
    })();
