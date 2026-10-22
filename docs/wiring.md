# Sơ đồ nối dây prototype MQ-2

Trạng thái: thiết kế để lắp và kiểm tra; chưa đo trên board. Áp dụng ESP32-DevKit V1 dùng ESP32 classic. Số chân dưới đây là GPIO, không phải vị trí header.

Các số GPIO trong sơ đồ là mặc định. Từ 27/09/2026 có thể đổi chín chân bằng [menuconfig](gpio-menuconfig.md); nếu đổi, nối dây theo cấu hình mới và kiểm tra log GPIO lúc boot.

Cập nhật phần mềm 26/09/2026: firmware đã tích hợp các module, debounce 30 ms và tùy chọn dò địa chỉ PCF8574/A. Dùng [flash-guide.md](flash-guide.md) để build/nạp và thử; các câu hướng tới bước triển khai bên dưới là lịch sử thiết kế, không có nghĩa còn thiếu driver.

## 1. Nguồn và bảng kết nối

Phương án thử trên bàn: ESP32 nhận nguồn từ USB; module MQ-2 và LCD nhận nguồn 5 V ổn áp ngoài. Nối GND của hai nguồn với nhau. Không nối cực dương nguồn 5 V ngoài vào chân VIN/5V của ESP32 khi USB đang cấp nguồn. Nguồn ngoài đề xuất có khả năng cấp 1 A hoặc hơn, sau đó kiểm chứng điện áp/dòng thực tế; đây không phải số đo tiêu thụ của hệ thống. Không cấp bộ sưởi MQ-2 từ chân 3V3.

| Đầu thiết bị | Nối tới |
| --- | --- |
| MQ-2 VCC | +5 V ngoài |
| MQ-2 GND | GND chung |
| MQ-2 AO | R1 = 22 kΩ, sau đó đến nút ADC ở mục 2 |
| MQ-2 DO | Để hở, không dùng |
| LCD VCC | +5 V ngoài |
| LCD GND | GND chung |
| LCD SDA | HV1 trên bộ chuyển mức I2C hai chiều |
| LCD SCL | HV2 trên bộ chuyển mức I2C hai chiều |
| Bộ chuyển mức LV | ESP32 3V3 |
| Bộ chuyển mức HV | +5 V ngoài |
| Bộ chuyển mức GND | GND chung |
| Bộ chuyển mức LV1 | GPIO21 (SDA) |
| Bộ chuyển mức LV2 | GPIO22 (SCL) |
| Nút ON/+ | GPIO25 ↔ nút ↔ GND |
| Nút OFF/− | GPIO26 ↔ nút ↔ GND |
| Nút Auto/Manual | GPIO27 ↔ nút ↔ GND |
| Nút Edit/Save | GPIO32 ↔ nút ↔ GND |
| Tín hiệu relay | GPIO23, HIGH bật / LOW tắt |

Module relay được coi là đầu nhận logic theo phạm vi người dùng đã chốt. Không bổ sung thiết kế mạch kích cuộn dây hoặc yêu cầu xác minh model trong tài liệu này; sơ đồ không yêu cầu GPIO cấp dòng trực tiếp cho cuộn dây.

## Cảm biến phát hiện lửa D0

Nối chân D0 của module cảm biến lửa tới GPIO4 mặc định (hoặc GPIO_FIRE_D0 đã chọn trong menuconfig), VCC theo thông số module và GND chung với ESP32. Chọn cực tính trong `Gas_System` → `Cam bien lua`: mặc định D0 LOW nghĩa là phát hiện lửa.

**Kiểm tra điện áp trước khi nối:** ESP32 dùng mức logic 3,3 V. Đo mức HIGH của chân D0 so với GND; không nối trực tiếp nếu D0 có thể lên 5 V. Dùng bộ chia áp hoặc bộ chuyển mức phù hợp để giữ D0 không quá 3,3 V khuyến nghị. Firmware chỉ đọc và log trạng thái D0; module này hiện không làm đổi relay.

## 2. MQ-2 AO và GPIO34

```text
MQ-2 AO ── R1 22 kΩ (1%) ──┬──── GPIO34 / ADC1_CH6
                           │
                           ├── R2 10 kΩ (1%) ── GND
                           │
                           └── C1 100 nF ────── GND
```

R2 và C1 song song; C1 đặt sát GPIO34. Đề xuất tụ gốm và điện trở 1/4 W.

Tính theo mô hình nguồn AO lý tưởng:

- V_ADC = V_AO × 10/(22+10) = 0,3125 × V_AO.
- AO = 5,00 V → V_ADC = 1,5625 V.
- Kiểm tra biên thiết kế AO ≤5,25 V và điện trở ±1%: V_ADC tối đa = 5,25 × 10,1/(21,78+10,1) ≈1,663 V. Đây là giả định biên tính toán, không phải thông số đã đo của FC-22.
- R_th = 22 kΩ || 10 kΩ ≈6,875 kΩ; tau lý tưởng ≈0,688 ms, 5 tau ≈3,44 ms. Trở kháng nguồn AO thực tế có thể làm thay đổi đáp ứng.
- Đề xuất ADC attenuation 12 dB trong ESP-IDF 5.5.4, hiệu chuẩn ADC nếu cần mV. Chừa biên đo, không coi 3,3 V là toàn dải tuyến tính.

Giới hạn: tổng tải 32 kΩ có thể làm thay đổi AO nếu đầu ra module chưa được đệm. Đo AO trước/sau mắc chia áp và ghi sai khác; đặt ngưỡng trên cấu hình mạch cuối cùng. Nếu cần giữ đặc tính đo chính xác hơn, đánh giá buffer sau khi có số đo tải. Chia áp này không phải mạch bảo vệ quá áp với mọi lỗi nguồn, và không biến ADC thành phép đo ppm đã hiệu chuẩn.

Khi kiểm tra, rút dây GPIO34 khỏi nút ADC trước. Kiểm tra riêng chia áp với điện áp DC biết trước (không phát tán gas để tạo điểm thử), đo điện áp nút ADC rồi mới nối ESP32 đã có nguồn. Không đưa tín hiệu vào ESP32 đang tắt nguồn.

## 3. LCD PCF8574 qua chuyển mức I2C

```text
ESP32                     Chuyển mức I2C              LCD backpack
3V3 ───────────────────── LV
+5 V ngoài ────────────── HV ──────────────────────── VCC
GPIO21 ────────────────── LV1  ⇄  HV1 ─────────────── SDA
GPIO22 ────────────────── LV2  ⇄  HV2 ─────────────── SCL
GND ───────────────────── GND ────────────────────── GND
```

Dùng module chuyển mức hai chiều phù hợp bus open-drain (ví dụ loại BSS138), không dùng bộ chia áp một chiều cho SDA. Bắt đầu I2C ở 100 kHz theo giới hạn PCF8574. Mỗi phía bus phải có pull-up về nguồn của phía đó; kiểm tra điện trở có sẵn trên translator/backpack trước khi thêm. Khi bus không hoạt động, phía ESP32 phải ở khoảng 3,3 V, phía LCD khoảng 5 V.

Địa chỉ chưa chốt. Khi có chương trình thử sẽ scan I2C; PCF8574 có dải địa chỉ 7-bit 0x20–0x27, PCF8574A 0x38–0x3F. Không mặc định 0x27. Scan xác định địa chỉ nhưng không xác định mapping P0–P7 của backpack; mapping phải được xác nhận riêng trước kiểm tra ký tự. Xoay biến trở để chỉnh tương phản sau khi cấp nguồn.

## 4. Bốn nút và đầu ra relay

Mỗi nút dùng sơ đồ sau, lặp lại cho GPIO25/26/27/32:

```text
3V3 ── 10 kΩ ──┬── GPIO nút
                │
              nút nhấn
                │
               GND
```

Thả nút = HIGH; nhấn = LOW. Với nút 4 chân, dùng đồng hồ chọn hai chân chỉ thông mạch khi nhấn, tránh nối nhầm hai chân vốn đã nối nội bộ. Debounce sẽ nằm trong button_task, chưa chốt thời gian tại bước nối dây.

```text
GPIO23 ────────┬── Tín hiệu điều khiển relay (active-high)
               └── 10 kΩ ── GND
```

Pull-down đặt gần ESP32 để định nghĩa mức nghỉ; firmware sẽ đặt LOW trước khi cho đầu ra hoạt động. Không tuyên bố đã kiểm chứng mức reset trên module.

Nhánh LED qua tiếp điểm thường hở:

```text
ESP32 3V3 ── COM relay
              NO ── R_LED 1 kΩ ── Anode LED
                                  Cathode ── GND
              NC: không nối
```

R_LED = 1 kΩ là lựa chọn khởi đầu cho LED báo dòng thấp. Nếu V_f≈2 V thì I≈(3,3−2)/1000≈1,3 mA. LED xanh dương/trắng có thể sáng yếu ở 3,3 V; không giảm điện trở trước khi biết thông số LED. Điện trở không phân cực; LED có phân cực.

## 5. Linh kiện bổ sung

| Linh kiện | Số lượng | Mục đích |
| --- | ---: | --- |
| 22 kΩ ±1%, 1/4 W | 1 | Nhánh trên chia áp AO |
| 10 kΩ ±1%, 1/4 W | 6 | 1 chia áp + 4 pull-up nút + 1 pull-down relay |
| 1 kΩ, 1/4 W | 1 | Hạn dòng LED |
| Tụ gốm 100 nF | 3 | 1 lọc ADC + 2 decoupling dự phòng gần nguồn MQ-2/LCD |
| Tụ 10 µF, định mức ≥10 V | 2 | Bổ sung gần hai nhánh nguồn MQ-2/LCD nếu cần; đúng cực khi dùng tụ phân cực |
| Chuyển mức I2C hai chiều 3,3/5 V | 1 | Dùng 2 kênh SDA/SCL, ưu tiên module có pull-up |
| Nguồn 5 V ổn áp, khả năng cấp ≥1 A | 1 | MQ-2/LCD; kiểm tra điện áp và tải thực tế |
| Đồng hồ đo, breadboard, dây ngắn | Theo bộ | Kiểm tra trước khi nối GPIO |

Không đặt dây AO sát dây chuyển mạch hoặc vùng anten. Dẫn dòng nguồn module về điểm GND nguồn; tránh dùng dây mass tín hiệu ADC làm đường hồi dòng tải. Tụ không thay thế nguồn hoặc dây dẫn đủ khả năng cấp dòng.

## 6. Trình tự nghiệm thu G0

1. Khi chưa cấp nguồn: đối chiếu nhãn chân, điện trở, cực tụ/LED và đo không chập nguồn–GND. Kiểm tra không nối chung hai đường +5 V USB và nguồn ngoài.
2. Cấp ESP32 qua USB và module qua nguồn ngoài, nối GND chung; giữ dây AO→GPIO34 rời trong bước đo chia áp.
3. Đo 3V3, nguồn 5 V ổn định theo nhu cầu MQ-2 (tham khảo điều kiện nhà sản xuất 5,0±0,1 V), và mức nghỉ I2C hai phía. Ghi số đo thực tế, không điền giá trị danh định làm kết quả.
4. Kiểm tra chia áp bằng DC biết trước; tại 5 V dự kiến xấp xỉ 1,56 V. Kiểm tra AO module sau khi mắc tải; chỉ nối GPIO34 khi mức đã phù hợp.
5. Kiểm tra bốn nút chuyển HIGH/LOW; đo GPIO23 trong reset khi tới bước thử firmware, giữ kết quả là pending hiện tại.
6. Trong terminal ESP-IDF chạy `idf.py --version`, rồi build/flash chương trình bring-up ở lượt triển khai: scan LCD, thử 2 dòng, đọc ADC, thử nút và đầu ra. Lưu log file và so sánh ADC khi bật/tắt Wi-Fi.

G0 chưa đạt khi chưa có các phép đo và build/flash trên. Việc hoàn thành tài liệu nối dây không đồng nghĩa hoàn thành thử phần cứng. Có thể tiếp tục đặc tả G1 với các mục đo còn pending, nhưng chưa khẳng định phần cứng đã nghiệm thu.

## Nguồn đối chiếu

- GPIO/ADC1 và hạn chế chân: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/gpio.html
- ADC oneshot/attenuation: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/adc_oneshot.html
- Giảm nhiễu ADC: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/adc_calibration.html
- PCF8574/PCF8574A, mức logic và tốc độ: https://www.ti.com/lit/ds/symlink/pcf8574.pdf
- Thông số cảm biến MQ-2 (không thay cho schematic module FC-22): https://www.winsen-sensor.com/product/mq-2.html
