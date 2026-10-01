# Kế hoạch project MQ-2 — Fresher Embedded

Ngày: 2026-09-24. Vai trò: Solution Architect.
Điều chỉnh phạm vi mới nhất: người dùng giới hạn relay ở xuất GPIO active-high (HIGH bật, LOW tắt). Các mục xác minh module relay bên dưới không còn là điều kiện chặn thiết kế/triển khai phần mềm; không tuyên bố đã kiểm chứng điện. Người dùng đính chính LCD dùng backpack PCF8574; PDF Waveshare gửi trước đó không áp dụng. MQ-2 qua ảnh FC-22; xem hardware.md.
Trạng thái: Kế hoạch được tạo theo yêu cầu; chưa triển khai firmware, web hay scaffold.

## 1. Mục tiêu và giới hạn

Xây dựng prototype ESP32-DevKit V1 dùng ESP-IDF: đo MQ-2, hiển thị LCD 16x2, bốn nút, một relay module 5 V/10 A điều khiển LED; web trong cùng mạng Wi-Fi đồng bộ hai chiều cả trạng thái chỉnh ngưỡng.

Mục tiêu hồ sơ: chứng minh thiết kế có quyền sở hữu dữ liệu rõ, FreeRTOS/event-driven, xử lý lỗi, kiểm thử xác định và kết quả đo thực tế. Không tuyên bố thiết bị đạt chuẩn cảnh báo gas hoặc đo ppm chính xác khi chưa có hiệu chuẩn phù hợp.

Đề xuất phạm vi phiên bản đầu: web do ESP32 phục vụ, HTTP + WebSocket, giao diện HTML/CSS/JavaScript nhỏ; không đưa cloud, MQTT broker, OTA, app điện thoại, database và nhiều relay vào đường hoàn thành chính. Đây là lựa chọn của bản kế hoạch, không phải tính năng đã triển khai.

## 2. Yêu cầu đã chốt và truy vết

| ID | Yêu cầu | Bằng chứng nghiệm thu dự kiến |
| --- | --- | --- |
| R01 | Reset vào Auto, giữ ngưỡng lưu, relay tắt đến Ready | Log khởi động, kiểm tra đầu ra, test khởi động với số đo cao |
| R02 | Ready cần đủ thời gian ổn định và chuỗi mẫu mới hợp lệ | Test ngay trước/đúng hạn, mẫu lỗi xen kẽ; báo cáo xác định thông số |
| R03 | Manual điều khiển relay bằng nút/web; vẫn hiện cảnh báo/lỗi cảm biến | Unit test, demo hai nguồn lệnh khi vượt ngưỡng và khi lỗi |
| R04 | Auto dùng ngưỡng có hiệu lực; mất dữ liệu hợp lệ thì relay tắt và báo lỗi | Test ngưỡng, timeout dữ liệu, phục hồi; thử trên board |
| R05 | Auto→Manual giữ đầu ra; Manual→Auto đánh giá ngay, chưa Ready thì tắt | Bảng test chuyển chế độ |
| R06 | Web/nút cùng sửa một bản nháp; nhấn chỉnh lần nữa lưu và thoát | Demo chỉnh từ hai phía; test xung đột và lệnh cũ |
| R07 | Mất web giữ bản nháp; nút tiếp tục dùng; hết hạn không thao tác thì hủy | Test thời gian giả và ngắt/kết nối lại Wi-Fi |
| R08 | Lưu lỗi giữ ngưỡng cũ và bản nháp; cấu hình thiếu/hỏng thì báo cần cấu hình, chặn Auto và tắt relay | Giả lập lỗi storage; test khởi động không có cấu hình |
| R09 | Sau lỗi, chỉ Ready lại sau chuỗi mẫu hợp lệ; Auto đánh giá ngay | Test mẫu phục hồi cao/thấp và lỗi xen kẽ |
| R10 | Web/LCD phản ánh trạng thái ESP32; không báo thành công cho lệnh thất bại | Log có mã lệnh, test queue đầy, reconnect, từ chối lệnh |

## 3. Các điểm cần xác minh và đề xuất để giải quyết

Các lựa chọn dưới đây chưa phải yêu cầu người dùng đã chốt. Giải quyết ở G0/G1 trước khi viết logic phụ thuộc.

| Điểm mở | Hướng đề xuất / cách xác minh |
| --- | --- |
| Module MQ-2, LCD và relay thực tế | Xác định model/sơ đồ; LCD I2C hay parallel; relay active-low/high và tương thích logic 3,3 V; nguồn và điện áp tải LED |
| Nguồn và mức điện áp | Đo/xác minh đầu ra analog MQ-2, pull-up LCD, đầu vào relay trước nối ESP32; chọn chia áp/chuyển mức nếu cần; kiểm tra relay tắt cả lúc reset trước khi firmware chạy |
| ESP-IDF | Ghi phiên bản cài đặt, chọn và khóa phiên bản cho build/test; dùng tài liệu đúng phiên bản |
| Đơn vị đo | Đề xuất ADC raw cho bản đầu; mV nếu triển khai/kiểm chứng calibration ADC; không gọi là ppm. Ngưỡng và hysteresis dùng cùng đơn vị |
| Bốn nút | Đề xuất ON/+ và OFF/− theo ngữ cảnh, Auto/Manual, Edit/Save; khóa bật/tắt tay trong Auto; khóa đổi mode khi đang chỉnh |
| Ngưỡng nháp | Đề xuất không ảnh hưởng ngưỡng điều khiển cho đến khi lưu thành công |
| Đang lưu | Đề xuất tạm khóa sửa/lưu lặp và tạm dừng timeout chỉnh; tiếp tục lấy mẫu, điều khiển theo ngưỡng cũ; kết quả gắn đúng yêu cầu lưu |
| Manual khi cấu hình thiếu | Cần chọn: cho LED điều khiển tay với trạng thái cần cấu hình, hoặc chặn relay toàn cục; không suy ra từ quy tắc Auto |
| Hysteresis | Đề xuất bật tại T_on, tắt tại T_off < T_on, giữ đầu ra trong khoảng giữa; xác định đầu ra khi vừa vào Auto ở khoảng giữa |
| Thời gian và giới hạn | Xác định chu kỳ mẫu, warm-up sau bật nguồn, chuỗi mẫu hợp lệ, hạn dữ liệu, timeout chỉnh, debounce, bước ngưỡng, biên ngưỡng, độ chênh hysteresis |
| Lệnh đến sát timeout | Đề xuất kiểm tra hạn phiên chỉnh tại lúc xử lý trên ESP32; timestamp client không quyết định quyền hợp lệ |
| Truy cập LAN | Chốt phạm vi client, origin được phép và cơ chế bảo vệ lệnh thay đổi; không coi cùng Wi-Fi là xác thực; không lưu Wi-Fi secret vào repo |

Thời gian conditioning ban đầu theo nhà sản xuất phải được phân biệt với warm-up mỗi lần khởi động. Không dùng số đo ít dao động hoặc dưới ngưỡng làm bằng chứng duy nhất cho Ready. ADC đọc thành công không bảo đảm mọi lỗi cảm biến được phát hiện.

## 4. Ranh giới thành phần đề xuất

| Thành phần | Trách nhiệm | Ranh giới |
| --- | --- | --- |
| Control core | Chuyển trạng thái, đánh giá lệnh, threshold/hysteresis, timeout, quyết định đầu ra | Logic C nhận sự kiện và thời gian; không gọi mạng/LCD/flash trực tiếp |
| Control runtime | Sở hữu duy nhất trạng thái điều khiển, tiếp nhận sự kiện, phối hợp kết quả | FreeRTOS; không để callback web hay driver ghi trực tiếp trạng thái |
| Sensor | Đọc ADC, thời điểm lấy mẫu, lọc và báo chất lượng | Không tự điều khiển relay; không đồng nhất lỗi điện với ADC đọc lỗi |
| Local I/O | Debounce nút, xuất relay, hiển thị LCD | Phát ý định người dùng; hiển thị snapshot nhất quán |
| Settings | Đọc/kiểm tra/lưu cấu hình qua NVS và báo kết quả | Không đổi ngưỡng có hiệu lực trước khi lưu thành công |
| Web gateway/UI | Phục vụ giao diện, kiểm tra payload, gửi lệnh, nhận snapshot/reconnect | Không tự coi thao tác UI là kết quả đã được ESP32 xác nhận |

Luồng chính: nút/web/sensor → sự kiện → control runtime/core → quyết định đầu ra và snapshot → relay/LCD/web. Lưu cấu hình là yêu cầu và kết quả riêng, gắn đúng bản nháp được nộp.

Định hướng đồng bộ: bản sao thông điệp nhỏ, queue hữu hạn, command ID, revision riêng cho bản nháp, nhận diện phiên chỉnh/phiên boot. Lệnh đặt ngưỡng và Save kiểm tra phiên bản dự kiến. Lệnh tăng tương đối phải có chính sách chống xử lý lặp. Snapshot cho LCD và web cần được phát riêng; một queue nhiều consumer không tự broadcast.

Không chốt số task, priority, core affinity hoặc kích thước queue trước khi có ngân sách thời gian/tải. Queue đầy phải có kết quả rõ; telemetry hiển thị có thể gộp giá trị mới nhất, nhưng không tùy tiện bỏ mẫu dùng đánh giá Ready. Thời hạn mẫu phải được kiểm tra ngay cả khi không có sự kiện sensor mới.

Cấu trúc cập nhật theo yêu cầu tách thư mục từng FreeRTOS task và gom driver. Các thư mục task/driver được tạo với README mô tả trách nhiệm; chưa có mã nguồn hoặc đăng ký component build:

```text
Gas_System/
  main/                       # Khởi tạo và kết nối thành phần
  components/
    app_tasks/                # Một component chứa các task ứng dụng
      control_task/           # Sở hữu trạng thái, xử lý sự kiện, xuất relay
      sensor_task/            # Lấy mẫu MQ-2 theo chu kỳ, gửi dữ liệu
      button_task/            # Quét/debounce nút, phát sự kiện nút
      display_task/           # Nhận snapshot, cập nhật LCD
      storage_task/           # Nhận yêu cầu lưu, trả kết quả
      web_task/               # Worker gửi snapshot; callback nhận lệnh qua gateway
    drivers/                  # Một component driver, các module ở thư mục con
      mq2/                    # Đọc ADC; không quyết định ngưỡng/cảnh báo
      lcd1602/                # Giao tiếp LCD, chưa chốt I2C hay parallel
      buttons/                # Đọc mức GPIO nút
      relay/                  # Xuất GPIO theo cực tính module
    control_core/             # Logic độc lập task/phần cứng + test
    settings/                 # Dịch vụ cấu hình NVS, không phải driver ngoại vi
    web_gateway/              # Wi-Fi/HTTP/WebSocket và tài nguyên giao diện
  tests/                      # Test tích hợp và kịch bản board
docs/                         # Yêu cầu, thiết kế, wiring, báo cáo
```

Đây là sáu vai trò task dự kiến, không phải sáu task đã chạy. Chốt priority, stack, chu kỳ và queue ở G1 dựa trên ngân sách thời gian; web_task là worker ứng dụng, không thay thế task nội bộ HTTP server/Wi-Fi. Một module driver không cần một task riêng: relay được control_task gọi; lcd1602 được display_task gọi. control_core không phụ thuộc app_tasks/drivers để giữ khả năng unit test độc lập.

Khi triển khai build, app_tasks và drivers mỗi bên có CMakeLists.txt đăng ký rõ source/include ở các thư mục con; không giả định ESP-IDF tự tìm mọi thư mục lồng nhau. Mỗi task sẽ có file .c và public header riêng; chưa tạo API giả khi hợp đồng G1 chưa chốt.

## 5. Các giai đoạn và điều kiện hoàn thành

Thực hiện tuần tự G0→G7. Mỗi giai đoạn bàn giao bằng chứng; không lấy việc code đã viết xong làm tiêu chí hoàn thành. Chưa đặt lịch theo tuần vì chưa có thời gian khả dụng và tình trạng phần cứng.

### G0 — Xác minh phần cứng và môi trường

- Việc cần làm: kiểm kê module, điện áp, nguồn, pin; chọn ADC1 phù hợp cho ESP32 dùng Wi-Fi; ghi phiên bản ESP-IDF; xác nhận các điểm mở ảnh hưởng chức năng.
- Đầu ra: `docs/hardware.md`, bảng chân/nguồn, danh sách thông số TBD có cách đo, bổ sung đặc tả yêu cầu.
- Hoàn thành khi: có sơ đồ kết nối đã kiểm tra mức điện áp; build/flash/log được chương trình kiểm tra tối thiểu trong giai đoạn triển khai; relay không bật ngoài ý muốn lúc reset; không còn mâu thuẫn hành vi chặn G1.
- Giá trị học: datasheet, ADC, GPIO, reset, phân biệt nguồn module với mức logic.

### G1 — Đặc tả trạng thái và hợp đồng đồng bộ

- Việc cần làm: lập bảng trạng thái/sự kiện/điều kiện/kết quả; chốt mapping nút, Save đang chờ, cấu hình lỗi; xác định payload ở mức hợp đồng, lỗi trả về, thứ tự và vòng đời phiên/revision.
- Đầu ra: `docs/spec.md`, `docs/state-model.md`, `docs/protocol.md`, ma trận test liên kết R01–R10.
- Hoàn thành khi: mỗi yêu cầu có tiêu chí Given/When/Then; mỗi lệnh có kết quả chấp nhận/từ chối; có chính sách queue đầy, timeout, gửi lại, reconnect và reset; tham số chưa đo được có tên/đơn vị/phương pháp xác định.
- Giá trị học: state machine, single writer, idempotency, stale commands.

### G2 — Logic điều khiển và unit test xác định

- Việc cần làm: triển khai control core với số đo, thời gian và kết quả storage giả; kiểm thử cùng logic sẽ dùng trên ESP32.
- Đầu ra: control core, unit tests, log kết quả và ma trận truy vết cập nhật.
- Hoàn thành khi: test R01–R09 thuộc logic đều pass; gồm ngay trước/đúng/sau timeout, ngưỡng biên, lỗi và phục hồi, xung đột Save, lệnh cũ; test không phụ thuộc mạng/gas thật hoặc sleep thời gian thực.
- Giá trị học: phân tách logic/I/O, test doubles, kiểm thử biên. Dùng Unity của ESP-IDF; runner host là lựa chọn sau khi kiểm tra toolchain, không giả định sẵn Windows hỗ trợ mọi test target.

### G3 — Phần cứng và vận hành tại chỗ

- Việc cần làm: tích hợp ADC/MQ-2, debounce nút, LCD, relay; đo nhiễu và timing; kết nối vào logic đã kiểm thử; xác định thông số sensor bằng log.
- Đầu ra: driver/local I/O, wiring hoàn chỉnh, log ADC và timing, báo cáo lựa chọn lọc/hysteresis/readiness.
- Hoàn thành khi: bốn nút/LCD thực hiện đúng luồng; Auto/Manual, khởi động và lỗi dữ liệu hoạt động trên board; relay không rung ở chuỗi đầu vào kiểm thử; LCD phân biệt số đo, chế độ, trạng thái chỉnh và lỗi. Các thông số thực tế quan trọng đã có căn cứ.
- Giá trị học: đưa logic lên thiết bị, debounce, ADC và giới hạn phát hiện lỗi.

### G4 — Lưu cấu hình và xử lý lỗi lưu

- Việc cần làm: kết nối NVS, xác minh dữ liệu đọc, phản hồi lỗi; giữ nguyên ngưỡng cũ khi Save thất bại; kiểm tra trường hợp mất nguồn trong quá trình lưu.
- Đầu ra: settings component, test tích hợp lưu/đọc và bảng kết quả reset/mất nguồn.
- Hoàn thành khi: ngưỡng đã lưu khôi phục đúng sau reset; cấu hình thiếu/hỏng báo cần cấu hình; lỗi ghi không báo thành công; sau gián đoạn không sử dụng cấu hình không hợp lệ. Chỉ ghi khi Save, không ghi theo từng lần kéo/tăng.
- Giá trị học: persistence, lỗi từng phần, ranh giới giữa RAM và flash.

### G5 — Web LAN và đồng bộ hai chiều

- Việc cần làm: giao diện số đo/mode/relay/threshold, bốn nút và slider khi chỉnh; gateway WebSocket; phản hồi lệnh, snapshot, offline/reconnect và revision.
- Đầu ra: web chạy từ ESP32, protocol tests, demo thao tác hai chiều.
- Hoàn thành khi: thao tác hardware phản ánh lên web và ngược lại, kể cả bản nháp; số đo cũ không hiển thị như đang live; reconnect nhận snapshot đầy đủ; lệnh sai/ngoài biên/cũ bị từ chối rõ; mất web không dừng điều khiển cục bộ.
- Giá trị học: protocol, backpressure, trạng thái phân tán và UI chờ xác nhận.

### G6 — Kiểm thử tích hợp, tải và độ bền

- Việc cần làm: chạy ma trận R01–R10; phát lệnh đồng thời, làm đầy queue, ngắt Wi-Fi, giả lập sensor/storage lỗi, reset và nhiều phiên chỉnh; đo latency, heap và stack.
- Đầu ra: `docs/test-report.md`, log có timestamp/command ID/revision/kết quả, báo cáo giới hạn.
- Mục tiêu tạm đề xuất, cần chốt tại G1: UI số đo 2–5 lần/giây; p95 từ gửi lệnh web đến phản hồi áp dụng ≤300 ms trên LAN bình thường trong ít nhất 100 thao tác; chạy liên tục 2 giờ, 20 chu kỳ reconnect và 20 lần reset. Đây là mục tiêu kiểm thử, không phải kết quả hoặc bảo đảm hard realtime.
- Hoàn thành khi: toàn bộ test bắt buộc pass; không lệnh thất bại nào được báo thành công; không crash/watchdog reset hoặc xu hướng heap giảm không giải thích được; stack còn biên đo được; báo cáo nêu điều kiện mạng, cách đo và kết quả chậm nhất, không chỉ trung bình.
- Giá trị học: tái hiện lỗi, đo đạc, giới hạn tài nguyên và tính truy vết.

### G7 — Đóng gói portfolio

- Việc cần làm: viết README tái lập build/test/demo; sơ đồ hệ thống và wiring; quay demo ngắn; ghi quyết định/trade-off và giới hạn.
- Đầu ra: README, sơ đồ, test report, video demo, nội dung CV dựa trên bằng chứng.
- Hoàn thành khi: từ checkout sạch và đúng toolchain có thể build theo hướng dẫn; unit test có lệnh chạy rõ; demo Auto/Manual, chỉnh hai chiều, mất mạng, lỗi sensor, reboot giữ cấu hình; không có secret trong repo. Không ghi thành tích latency/độ chính xác chưa đo.
- Giá trị học: trình bày quyết định kỹ thuật, chứng minh kết quả.

## 6. Rủi ro và quy tắc kiểm chứng

- Không nối điện áp module vào ESP32 trước khi kiểm tra; 5 V/10 A trên relay không xác định điện áp input logic hay điện áp tải LED.
- Xác minh đầu ra OFF qua reset bằng phần cứng; firmware không kiểm soát mọi thời điểm trước app_main.
- Ngăn mạng/LCD/storage chặn vòng điều khiển quá ngân sách; kết quả Save gắn đúng yêu cầu; snapshot không đọc xé giữa nhiều biến.
- Chống lệnh cũ không chỉ bằng FIFO; quy định phạm vi cache chống lặp và phiên boot, không hứa exactly-once xuyên reset.
- Không sửa firmware để đoán lỗi: khi debug, ghi log tái hiện vào file trước, rồi sửa tối thiểu và chạy lại test liên quan.
- Ghi log cấu trúc cho chuyển trạng thái, lệnh từ chối, timeout, Save lỗi, queue đầy; không log secret và không flood log từng mẫu không cần thiết.
- Kết quả unit test không thay thế kiểm tra điện, ADC, reset hay kết nối trên thiết bị.

## 7. Nguồn kỹ thuật tham khảo

- ESP-IDF ADC oneshot (tham khảo ràng buộc ADC2/Wi-Fi, cần đối chiếu phiên bản đã khóa): https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/adc_oneshot.html
- ESP-IDF Unity unit testing: https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-guides/unit-tests.html
- HTTP/WebSocket server: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
- FreeRTOS queue: https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/01-Queues
- MQ-2 manufacturer specifications: https://www.winsen-sensor.com/product/mq-2.html

## 8. Trạng thái bàn giao kế hoạch

Đã đối chiếu yêu cầu với docs/context.md và kiểm tra mã nguồn hiện có: app_main còn trống. Chỉ tạo tài liệu kế hoạch và cập nhật core memory. Chưa build, chạy test, đo timing hay xác minh wiring trong lượt lập kế hoạch này. G0 là bước triển khai tiếp theo khi người dùng yêu cầu bắt đầu thực hiện.
