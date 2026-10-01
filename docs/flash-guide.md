# Bàn giao firmware: cấu hình, nối mạch và nạp

## Những gì đã viết xong

Đã tích hợp core Auto/Manual; task nút, cảm biến, LCD và NVS; driver GPIO/ADC/I2C; Wi-Fi STA và web LAN. Bản profile bàn giao bật driver thật và web. Không cần viết thêm module để thử mạch theo chức năng đã chốt.

Điều chưa thể làm khi không có board là đo điện áp, xác nhận mapping backpack, thời gian ổn định MQ-2 và chạy thử hệ thống thật. Việc build thành công không thay thế các phép thử này.

## 1. Tạo cấu hình cá nhân

Mở terminal **ESP-IDF 5.5.4 PowerShell** ở thư mục gốc repository. Sao chép mẫu một lần:

```powershell
Copy-Item Gas_System/config/board.example.json Gas_System/config/board.local.json
```

Sửa `Gas_System/config/board.local.json` bằng editor:

| Trường | Điền gì |
| --- | --- |
| wifi_ten | SSID Wi-Fi 2,4 GHz, 1–32 byte UTF-8 |
| wifi_mat_khau | Mật khẩu mạng WPA2/WPA3 Personal, 8–63 byte UTF-8 |
| mq2_on_dinh_giay | Thời gian chờ đã xác định, đơn vị giây; 0 nghĩa chưa xác định |
| lcd_tu_tim | true: dò địa chỉ trong 0x20–0x27 và 0x38–0x3f; chỉ chọn khi đúng một thiết bị phản hồi |
| lcd_dia_chi | Khi tắt tự dò: số thập phân, 39 tương ứng 0x27, 63 tương ứng 0x3f |
| lcd_so_do | Chuỗi 8 chữ số; mặc định 76543210 tương ứng mapping phổ biến P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7 |
| lcd_den_muc_cao | true nếu bit backlight HIGH bật đèn nền |

File JSON không cho phép comment. Dùng dấu ngoặc kép cho chuỗi; dấu `"` và `\` bên trong phải escape đúng cú pháp JSON. Bộ tạo profile kiểm tra cấu hình trước khi gọi build. Thông tin Wi-Fi được tạo thành header trong thư mục build riêng; file cấu hình cá nhân và thư mục build được gitignore. Không đưa firmware chứa thông tin Wi-Fi cá nhân lên nơi công khai.

**Bạn chưa cần đoán thời gian MQ-2.** Để 0 vẫn build/nạp được để thử web, nút, LCD, chỉnh ngưỡng và relay Manual. Cảm biến chưa Ready, Auto giữ relay tắt. Serial có `su_kien=chua_cau_hinh_on_dinh_mq2 san_sang=bi_khoa`; LCD/web báo đang chờ ổn định. Log `cam_bien` vẫn cung cấp `tho`, `loc`, `hop_le` để quan sát tín hiệu.

Sau khi xác định được thời gian chờ, điền giây vào file rồi build/nạp lại. Firmware đợi hết thời gian kể từ lúc core khởi động và đủ chuỗi mẫu hợp lệ trước khi Ready. Giá trị này không thay thế quá trình làm nóng ban đầu hoặc hiệu chuẩn. [Tài liệu Winsen MQ-2](https://www.winsen-sensor.com/d/files/newpdf/mq-2-%28ver1_6%29---manual.pdf) nêu thời gian làm nóng ít nhất 48 giờ trong điều kiện thử chuẩn; không suy ra một thời gian khởi động ngắn cho module FC-22 chưa đo.

## 2. Build trước khi có board

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Build
```

Lệnh tạo/đọc `Gas_System/config/sdkconfig.board`, header Wi-Fi riêng và build tại `Gas_System/build_ready`. Luôn tắt `DK_THU_NGHIEM`, bật web và áp dụng các giá trị LCD/MQ-2 từ JSON. GPIO đã lưu qua menuconfig được giữ nguyên. `Gas_System/sdkconfig` gốc không bị sửa bởi bộ tạo profile. Header sinh trong build được ưu tiên hơn header Wi-Fi cũ trong source.

Để đổi chân, chạy cùng script với `-Action Menuconfig`, vào `Chan GPIO - Gas System (ESP32 DevKit V1)`. Chi tiết: [gpio-menuconfig.md](gpio-menuconfig.md).

Thành phẩm chính: `build_ready/Gas_System.bin`, `Gas_System.elf`, bootloader và partition table. Nạp bằng bước 4 để đúng các offset; không chỉ chép riêng file ứng dụng vào offset 0.

Nếu PowerShell báo chưa có IDF_PATH, hãy mở đúng ESP-IDF terminal hoặc chạy `export.ps1` của bản ESP-IDF đã cài. Nếu build thất bại, dừng và đọc lỗi; không nạp binary còn sót từ lần trước.

## 3. Nối mạch

Đọc [wiring.md](wiring.md) trước khi cấp nguồn. Bảng GPIO mặc định (có thể đổi qua menuconfig):

| Chức năng | GPIO |
| --- | ---: |
| MQ-2 AO qua chia áp | 34 / ADC1_CH6 |
| LCD SDA / SCL qua chuyển mức I2C | 21 / 22 |
| Bật hoặc tăng | 25 |
| Tắt hoặc giảm | 26 |
| Auto/Manual | 27 |
| Chỉnh/Lưu ngưỡng | 32 |
| Relay active-high | 23 |

MQ-2 AO không nối thẳng vào ESP32 khi có khả năng lên 5 V. LCD 5 V dùng chuyển mức I2C theo sơ đồ. Các nút nối xuống GND khi nhấn. Đèn LED có điện trở, dùng nhánh 3,3 V qua COM/NO như sơ đồ đã chốt.

Tự dò LCD chỉ xác định địa chỉ có ACK, không chứng minh đó là LCD và không xác định mapping P0–P7. Profile này giả định LCD là thiết bị duy nhất trong các dải PCF trên bus. Nếu có nhiều thiết bị, chọn địa chỉ rõ ràng trong cấu hình. Nếu màn hình có nền nhưng không có chữ, kiểm tra tương phản, mapping và dây trước khi đổi source.

## 4. Nạp và mở serial

Xem cổng USB serial của ESP32 trong Device Manager. Ví dụ COM5:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Flash -Port COM5
```

Lệnh build lại với cấu hình cá nhân, flash rồi mở monitor. Không tự chọn cổng, không tự erase toàn bộ NVS. Thoát IDF monitor bằng `Ctrl+]`. Nếu board không tự vào download mode, giữ BOOT khi esptool bắt đầu kết nối rồi thả khi nạp bắt đầu.

Mở lại monitor khi cần:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Monitor -Port COM5
```

## 5. Thử lần đầu

1. Kiểm tra log đang dùng ADC thật, không có `che_do=gia_lap`. Relay khởi động OFF, mode Auto.
2. LCD hiển thị trạng thái. Khi chưa từng lưu, `CAN DAT NGUONG` là bình thường. Bấm Edit, dùng +/- chọn ngưỡng, bấm Edit lần nữa để lưu. Số 600 là bản nháp khởi đầu, không phải ngưỡng gas đã hiệu chuẩn.
3. Bấm chuyển Manual, thử bật/tắt LED. Lúc chỉnh ngưỡng, hai nút ON/OFF đổi thành +/-10; nút mode bị từ chối trong phiên chỉnh theo quy tắc core.
4. Tìm log `su_kien=co_ip url=http://...`, mở địa chỉ đó từ thiết bị cùng LAN. Web có bốn nút và slider. Đổi từ web rồi quan sát LCD; đổi từ nút rồi quan sát web.
5. Ngắt Wi-Fi: nút/LCD tiếp tục hoạt động; web báo mất đồng bộ. Nối lại: web lấy trạng thái ESP32, không phát lại lệnh cũ.
6. Reset sau khi lưu thành công: ngưỡng được khôi phục, trở về Auto/OFF và đợi điều kiện cảm biến sẵn sàng. Không giữ relay ON qua reset.
7. Sau khi đã xác định thời gian ổn định và chọn ngưỡng theo mạch thực tế, thử logic Auto bằng tín hiệu thử phù hợp. Không dùng số ADC giả lập hoặc một lần đọc để kết luận nồng độ ppm.

## 6. Tra lỗi nhanh

| Hiện tượng | Kiểm tra |
| --- | --- |
| Không build được cấu hình | Đủ trường JSON, SSID/password đúng độ dài; xem mẫu |
| Không có IP | Wi-Fi 2,4 GHz, mật khẩu, vùng phủ sóng; firmware thử kết nối lại |
| Có IP nhưng không mở web | Thiết bị cùng LAN, router không bật client isolation, dùng http:// |
| Auto luôn OFF | Đã lưu ngưỡng chưa, mq2_on_dinh_giay có bằng 0 không, mẫu hợp lệ chưa, đã vượt ngưỡng chưa |
| Lỗi LCD / không chọn địa chỉ | Nguồn, GND, chuyển mức, SDA/SCL; log nhieu_dia_chi cần đặt địa chỉ cụ thể |
| ADC 0/4095 hoặc lỗi cảm biến | Dây AO, chia áp, nguồn, log ADC; firmware không hiển thị lỗi thành an toàn |
| Lưu lỗi | Đọc log NVS; không tự erase vì thao tác đó xóa cấu hình |

## Kiểm chứng trước bàn giao

Chạy `Gas_System/tests/run_all.ps1`: core, runtime, nút, storage, sensor, LCD, gateway/network/UI và bộ tạo profile. `tests/build_firmware.ps1 -ThuBanGiao` dùng cấu hình Wi-Fi giả riêng để kiểm tra liên kết đầy đủ driver + mạng + web; không kết nối mạng hoặc flash. Các giá trị Wi-Fi trong fixture chỉ dùng cho build, không phải cấu hình của bạn.

Phần còn lại là nghiệm thu trên phần cứng: mức điện áp, mapping thực, nhiễu ADC khi Wi-Fi/relay hoạt động, điều kiện Ready, độ trễ và độ ổn định chạy lâu. Không cần triển khai thêm PC simulator để nạp firmware này.
