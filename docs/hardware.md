# Phần cứng — G0

Trạng thái: đã có thiết kế nối dây trong [wiring.md](wiring.md); chưa chạy kiểm tra điện hoặc nghiệm thu G0.

| Thành phần | Người dùng xác nhận | Còn cần xác minh |
| --- | --- | --- |
| MCU | ESP32-DevKit V1, ESP-IDF | Sơ đồ/chân của board thực tế |
| LCD | LCD1602 dùng backpack PCF8574, theo đính chính mới nhất của người dùng | Địa chỉ I2C, mapping backpack và nguồn/pull-up thực tế |
| MQ-2 | Module AO/DO; ảnh có chữ FC-22 và nhãn VCC/GND/DO/AO | Ảnh không xác minh được biên điện áp AO hoặc sơ đồ đầy đủ |
| Relay | Active-high: HIGH bật, LOW tắt | Theo yêu cầu mới, chỉ xét hợp đồng GPIO; không yêu cầu thêm model hoặc thông số module |
| Tải | LED rời kèm điện trở, nguồn nhánh LED 3,3 V | Điện áp thuận LED, giá trị điện trở |

## Định hướng driver

- `drivers/lcd1602`: điều khiển LCD qua backpack PCF8574. Địa chỉ I2C và mapping chưa được kiểm chứng; xác minh khi bring-up. Không áp dụng thông số địa chỉ, nguồn hoặc RGB từ PDF Waveshare đã gửi nhầm. Chưa chốt nguồn LCD và pull-up I2C.
- `drivers/mq2`: đề xuất dùng AO qua ADC cho số đo/ngưỡng phần mềm; DO chưa cần dùng trong phiên bản đầu. Kiểm tra mức điện áp trước khi nối AO.
- `drivers/relay`: API biểu diễn ON/OFF; ON = HIGH, OFF = LOW. Theo chỉ đạo mới của người dùng, thiết kế giới hạn tại GPIO, không lấy việc xác minh module relay làm điều kiện chặn phần mềm; không tuyên bố đã kiểm chứng điện.
- `drivers/buttons`: đọc GPIO; task xử lý debounce, core quyết định ý nghĩa theo chế độ.
- Thiết kế nối dây hiện tại: GPIO theo bảng dưới, chia áp AO 22 kΩ/10 kΩ ±1% và tụ 100 nF; LCD cấp 5 V qua chuyển mức I2C hai chiều; nút pull-up 10 kΩ, relay pull-down 10 kΩ, LED nối tiếp 1 kΩ. Các giá trị là lựa chọn thiết kế chờ kiểm chứng, không phải thông số đo.

## Bảng GPIO đề xuất cho ESP32-DevKit V1

Áp dụng ESP32 classic (module ESP32-WROOM thường gặp trên DevKit V1), không áp dụng tự động cho S2/S3/C3. Dùng số GPIO in trên board, không đếm vị trí chân. Đây là đề xuất đã đối chiếu tài liệu chip, chưa đo trên board.

| Chức năng | GPIO | Cấu hình dự kiến |
| --- | --- | --- |
| MQ-2 AO | 34 | ADC1_CH6, chỉ input; qua mạch điều chỉnh mức điện áp, không nối thẳng AO 5 V |
| LCD SDA | 21 | I2C SDA, kiểm tra pull-up/mức điện áp |
| LCD SCL | 22 | I2C SCL; đề xuất bắt đầu 100 kHz |
| Nút ON/+ | 25 | Input pull-up, nhấn nối GND |
| Nút OFF/− | 26 | Input pull-up, nhấn nối GND |
| Nút Auto/Manual | 27 | Input pull-up, nhấn nối GND |
| Nút Edit/Save | 32 | Input pull-up, nhấn nối GND |
| Relay | 23 | Output active-high; LOW là OFF |
| MQ-2 DO | Không nối | Dùng ngưỡng phần mềm trên AO |

- Tránh GPIO0/2/5/12/15 cho ngoại vi phiên bản đầu vì strapping; tránh GPIO6–11 vì flash; giữ GPIO1/3 cho nạp/log và GPIO12–15 cho JTAG. GPIO16/17 phụ thuộc module/PSRAM nên không cần chọn trong cấu hình này.
- GPIO34–39 chỉ input, không có pull-up/pull-down nội; GPIO34 phù hợp ADC nhưng không dùng làm relay/I2C output.
- ADC2 xung đột Wi-Fi khi đo analog; GPIO25/26/27 vẫn dùng GPIO digital cho nút bình thường. Chọn ADC1 cho MQ-2 không bảo đảm số đo hoàn toàn hết nhiễu.
- AO cần chia áp hoặc buffer/chia áp theo điện áp cực đại và dải ADC đã chọn; tính cả tải lên module và dung sai. Không giả định ADC đo tuyến tính toàn dải 0–3,3 V; không suy ra ppm từ ADC calibration.
- Đề xuất tụ 100 nF từ nút ADC sau chia áp xuống GND, gần ESP32; kiểm tra thời gian ổn định RC. Lấy nhiều mẫu/lọc trong phần mềm, đo nhiễu khi Wi-Fi và đầu ra hoạt động.
- Dây analog ngắn, đi cùng GND, xa dây chuyển mạch/antenna; không để dòng nguồn tải chạy qua đoạn mass tín hiệu ADC. Nguồn ổn định, chung tham chiếu GND cho tín hiệu.
- Nếu LCD/backpack chạy 5 V, dùng chuyển mức I2C hai chiều phù hợp giữa bus 3,3 V và 5 V. Không để pull-up 5 V lên chân ESP32; chỉ chuyển pull-up sang 3,3 V chưa bảo đảm VIH của PCF8574 chạy 5 V.
- Nút: pull-up nội đủ để thử dây ngắn; đề xuất pull-up ngoài 10 kΩ lên 3,3 V cho dây breadboard dài hơn, kèm debounce. Relay: có thể dùng pull-down ngoài 10 kΩ ở GPIO23 để xác định LOW lúc reset, cần kiểm chứng mức thực tế; không mở rộng điều tra module relay ngoài phạm vi người dùng.

Nguồn đối chiếu:
- https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/gpio.html
- https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/adc_calibration.html
- https://www.ti.com/lit/ds/symlink/pcf8574.pdf

## Môi trường công cụ đã quan sát

- `Gas_System/sdkconfig` dòng đầu ghi cấu hình được tạo bởi ESP-IDF 5.5.4; target esp32.
- `Get-Command idf.py` không tìm thấy lệnh trong PowerShell hiện tại. Điều này không chứng minh ESP-IDF chưa được cài; có thể terminal chưa nạp môi trường.
- Chưa chạy build/flash hoặc đo phần cứng.

## Tiếp theo

1. Lắp/kiểm tra theo wiring.md và ghi số đo; kiểm tra địa chỉ LCD khi bring-up. Không yêu cầu thêm thông tin module relay.
2. Nạp terminal ESP-IDF và kiểm tra phiên bản công cụ thực tế.
3. Kiểm chứng mức điện áp tại ADC, I2C và các đầu GPIO theo sơ đồ đã viết.
4. Hoàn thành G0 theo project-plan.md trước khi triển khai driver phụ thuộc wiring.

## Nguồn và giới hạn chứng cứ

- Ảnh LCD bổ sung từ người dùng: thấy LCD1602 với backpack I2C, đầu nối 4 chân và biến trở chỉnh tương phản. Mã IC không đọc rõ; loại PCF8574 dựa trên xác nhận của người dùng. Không suy ra địa chỉ I2C, mapping hoặc điện áp pull-up từ ảnh. Hình gồm biến thể nền vàng-xanh và xanh dương, không phải bằng chứng điều khiển RGB.

- Người dùng đính chính LCD là loại backpack PCF8574. PDF Waveshare gửi trước đó không áp dụng cho phần cứng này.
- Ảnh MQ-2 do người dùng cung cấp: quan sát được FC-22, biến trở và chân VCC/GND/DO/AO; không đủ để kết luận đặc tính điện hay nhà sản xuất cảm biến.
