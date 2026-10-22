# Đổi chân qua menuconfig

Tại thư mục gốc repository, mở ESP-IDF 5.5.4 PowerShell terminal. Nếu dùng bộ công cụ bàn giao, điền Wi-Fi trong `Gas_System/config/board.local.json` như [flash-guide.md](flash-guide.md), sau đó chạy:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Menuconfig
```

Mở **Chan GPIO - Gas System (ESP32 DevKit V1)**, nhập số GPIO cần dùng, nhấn **S** để lưu và thoát. Đây là số GPIO trên board, không phải số thứ tự chân header.

Để đổi hysteresis, vào `Gas_System` → `Dieu khien` → `Do tre nguong hysteresis MQ-2 (ADC tho)`. Mặc định là 50 và giới hạn 1–100 số đếm ADC thô. Tham số này dùng chung cho điều kiện tắt relay Auto theo gas và điều kiện hồi phục cảnh báo gas Telegram; không áp dụng cho ngõ D0 phát hiện lửa.

| Mục | Mặc định | Chức năng |
| --- | ---: | --- |
| GPIO MQ2 | 34 | AO qua chia áp, dùng ADC1 |
| GPIO LCD_SDA | 21 | SDA qua chuyển mức I2C |
| GPIO LCD_SCL | 22 | SCL qua chuyển mức I2C |
| GPIO RELAY | 23 | HIGH bật, LOW tắt |
| GPIO NUT_BAT | 25 | Bật / tăng |
| GPIO NUT_TAT | 26 | Tắt / giảm |
| GPIO NUT_CHE_DO | 27 | Auto / Manual |
| GPIO NUT_CHINH | 32 | Chỉnh / lưu ngưỡng |
| GPIO FIRE_D0 | 4 | D0 cảm biến lửa; input số |

Trong menu **Cam bien lua**, bật `D0 muc LOW khi phat hien lua` nếu module kéo D0 xuống LOW khi phát hiện lửa (mặc định). Tắt lựa chọn này nếu module xuất HIGH khi phát hiện. GPIO FIRE_D0 phải khác mọi GPIO đang dùng.

Build/nạp lại để firmware sử dụng chân mới:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Build
powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tools/firmware.ps1 -Action Flash -Port COM5
```

Đổi COM5 thành cổng của board. Chuyển dây theo cấu hình đã lưu trước khi thử phần cứng. Khi boot, log `su_kien=cau_hinh_gpio` in các chân đang sử dụng.

## Chân được phép trong project

- MQ-2: **32, 33, 34, 35, 36, 39**. Kênh ADC1 tự ánh xạ lần lượt thành 4, 5, 6, 7, 0, 3. Chỉ dùng ADC1 trong thiết kế có Wi-Fi.
- Relay, LCD và nút: **4, 13, 14, 16–19, 21–23, 25–27, 32, 33**.
- FIRE_D0: các GPIO digital input ở danh sách trên, hoặc **34–39** (input-only; module phải tự định mức D0 vì các chân này không có pull nội). Đây là chính sách của project cho ESP32 classic/WROOM: tránh chân flash, UART0 và strapping; nút dùng pull-up nội.
- Chín chức năng GPIO phải dùng chín chân khác nhau. Ví dụ chuyển MQ-2 sang GPIO32 thì phải chuyển NUT_CHINH ra chân khác.
- Không dùng GPIO34–39 cho output hoặc nút có pull-up nội. Chúng chỉ có khả năng input và không có pull-up/pull-down phần mềm. [Tài liệu GPIO Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/peripherals/gpio.html).
- GPIO16/17 chỉ dùng khi module không dành chúng cho PSRAM; hướng dẫn này dành cho DevKit V1 với ESP32/WROOM. Không áp dụng nguyên bộ chân cho ESP32-S3/C3 hoặc module khác.

Menu cho nhập số trong khoảng; `board_pins.h` kiểm tra chính xác tập chân và xung đột khi biên dịch. Nếu sai, compiler báo `GPIO ... khong hop le` hoặc `Trung GPIO: ...`; sửa menu rồi build lại, không nạp binary cũ.

## Nơi lưu cấu hình

Với `firmware.ps1`, cấu hình ESP-IDF cá nhân nằm ở **Gas_System/config/sdkconfig.board** (gitignore), ngoài thư mục build. Các lần Build/Flash đọc lại file này nên không ghi đè lựa chọn GPIO. Dọn build không xóa file cấu hình. JSON vẫn sở hữu Wi-Fi, thời gian ổn định MQ-2, địa chỉ/mapping/đèn nền LCD; các mục đó sửa trong JSON, không phải cấu hình GPIO.

Nếu chạy ESP-IDF trực tiếp thay vì công cụ bàn giao, từ `Gas_System` dùng `idf.py menuconfig` rồi `idf.py build`: các lệnh này dùng `Gas_System/sdkconfig`. Hai cách dùng hai file khác nhau; không chỉnh file của cách này rồi nạp bằng cách kia và kỳ vọng tự đồng bộ.

Luồng source: `drivers/Kconfig.projbuild` → `sdkconfig.h` → `drivers/board_pins.h` → driver MQ-2/I2C/GPIO. File chính vẫn khởi tạo task/driver như trước; driver lấy số chân từ cấu hình mới.
