# Cấu hình và kiểm tra Telegram Bot

Hướng dẫn này áp dụng cho firmware hiện tại trong `Gas_System`. ESP32 gửi tin nhắn đi qua HTTPS đến Telegram; Telegram không cần truy cập vào ESP32. ESP32 phải có Wi-Fi và đường ra Internet. Log nhận được địa chỉ IP chỉ xác nhận đã kết nối mạng nội bộ, không xác nhận Telegram đang truy cập được.

## 1. Tạo bot và mở cuộc trò chuyện

1. Mở Telegram, tìm tài khoản chính thức [@BotFather](https://t.me/BotFather) và gửi `/newbot`.
2. Làm theo hướng dẫn để đặt tên và username. BotFather trả về **bot token**. Hãy coi token như mật khẩu: ai có token có thể điều khiển bot.
3. Tìm bot mới theo username, mở cuộc trò chuyện và gửi `/start`. Bot cần được người nhận mở trước thì mới có thể gửi tin nhắn riêng cho người đó.

## 2. Lấy Chat ID số

Firmware hiện chấp nhận Chat ID dạng chữ số, có thể bắt đầu bằng dấu `-` cho một số nhóm. Firmware không chấp nhận username kênh dạng `@tenkenh`.

Sau khi gửi `/start` cho bot, chạy đoạn PowerShell dưới đây trên máy cá nhân. `Read-Host -AsSecureString` không hiện token khi gõ. Đoạn lệnh chỉ in loại cuộc trò chuyện và ID; không in toàn bộ nội dung cập nhật Telegram.

```powershell
$tokenSecure = Read-Host "Nhap bot token" -AsSecureString
$tokenPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($tokenSecure)
$botToken = $null
$requestUri = $null
$updates = $null

try {
    $botToken = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($tokenPointer)
    $requestUri = "https://api.telegram.org/bot$botToken/getUpdates"
    $updates = Invoke-RestMethod -Uri $requestUri -Method Get -ErrorAction Stop

    $chats = foreach ($update in $updates.result) {
        if ($null -ne $update.message -and $null -ne $update.message.chat) {
            [pscustomobject]@{
                chat_id = $update.message.chat.id
                type    = $update.message.chat.type
            }
        }
    }

    if ($chats) {
        $chats | Sort-Object chat_id -Unique | Format-Table
    }
    else {
        Write-Host "Chua co update. Hay gui /start cho bot roi chay lai lenh."
    }
}
catch {
    Write-Host "Khong doc duoc update. Kiem tra mang va token; khong chia se noi dung loi tho."
}
finally {
    if ($tokenPointer -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($tokenPointer)
    }
    $botToken = $null
    $requestUri = $null
    $updates = $null
    if ($null -ne $tokenSecure) {
        $tokenSecure.Dispose()
    }
    $tokenSecure = $null
}
```

Chép đúng số ở cột `chat_id`; không chép dấu ngoặc hay khoảng trắng. Nếu đã cấu hình webhook cho bot, Telegram không cho dùng `getUpdates` đồng thời; với bot mới thường chưa cần xử lý trường hợp này. [Telegram Bot API: getUpdates](https://core.telegram.org/bots/api#getupdates)

## 3. Nhập cấu hình trong menuconfig

Mở PowerShell trong thư mục `Gas_System`, rồi chạy:

```powershell
idf.py menuconfig
```

Trong menu `Gas_System`:

1. Mở `Wi-Fi`, nhập SSID và mật khẩu của mạng có Internet nếu chưa cấu hình.
2. Mở `Telegram Bot`.
3. Bật `Bat thong bao Telegram`.
4. Nhập token vào `Bot token` và số vào `Chat ID nhan tin`.
5. Lưu cấu hình rồi thoát menuconfig.

Sau đó build và nạp firmware:

```powershell
idf.py build
idf.py -p COMx flash monitor
```

Thay `COMx` bằng cổng serial của board. Nếu monitor đang chạy, thoát bằng `Ctrl+]`. `menuconfig` lưu giá trị vào `Gas_System/sdkconfig`; file này bị Git ignore trong project để tránh commit nhầm bí mật, nhưng nội dung trên máy vẫn là văn bản thuần.

## 4. Đọc log và xác nhận gửi

Khi khởi động, tìm các log sau:

- `su_kien=telegram_worker ket_qua=san_sang`: worker đã khởi động, cấu hình hợp lệ.
- `su_kien=telegram_worker ket_qua=khong_san_sang`: worker không khởi động. Kiểm tra token, Chat ID và mã lỗi; phần điều khiển relay vẫn tiếp tục chạy.
- `su_kien=telegram_worker ket_qua=tat_trong_menuconfig`: tính năng đang tắt.

Log worker sẵn sàng chỉ xác nhận task và cấu hình khởi tạo được; nó không kiểm tra Internet hay xác nhận Telegram đã nhận tin.

Telegram chỉ gửi sau khi MQ-2 đạt trạng thái Ready và control core phát sinh chuyển trạng thái cảnh báo hoặc hồi phục. Điều kiện cảnh báo là ADC thô lớn hơn hoặc bằng ngưỡng hiệu lực. Điều kiện hồi phục là ba mẫu hợp lệ liên tiếp không lớn hơn `ngưỡng - 50`. Hai loại thông báo áp dụng ở cả Auto và Manual. Giá trị gửi đi là ADC thô, không phải ppm.

Khi có chuyển trạng thái, monitor lần lượt có thể hiển thị:

```text
su_kien=chinh_sach_canh_bao chuyen=VUOT_NGUONG ...
event=submit result=accepted kind=alarm ...
event=send result=success kind=alarm ...
```

Hồi phục dùng `chuyen=ON_DINH` và `kind=recovery`. `event=submit result=accepted` chỉ có nghĩa sự kiện được nhận vào worker; chỉ `event=send result=success` xác nhận Telegram API trả về thành công. Nếu có lỗi mạng, HTTP/API, hàng đợi đang bận hoặc cấu hình sai, sự kiện đó bị bỏ và không gửi bù/thử lại. Lỗi gửi thường có log `event=send result=drop`; xem `http`, `error` và `api_ok`. Log lỗi khởi động có thể có `reason`. Không đưa token vào log.

Không thấy log sự kiện chưa chắc Telegram lỗi: có thể cảm biến chưa Ready, chưa vượt ngưỡng hoặc chưa đủ ba mẫu hồi phục. Không cần và không nên cố tình tạo khí dễ cháy để thử cảm biến.

## 5. Bảo vệ token

- Không gửi token qua chat, issue, ảnh chụp monitor hoặc kho Git.
- `sdkconfig` bị ignore nhưng vẫn lưu token dưới dạng văn bản thuần trên máy.
- Firmware cần token lúc chạy nên token cũng được đưa vào firmware đã build; người có quyền đọc firmware có thể trích xuất nó. Đây phù hợp demo học tập, không phải nơi lưu bí mật có yêu cầu bảo mật cao.
- Nếu token bị lộ, dùng BotFather để thu hồi/tạo token mới, cập nhật menuconfig, build và nạp lại firmware. Xóa token khỏi file hiện tại không làm token cũ trong firmware đã nạp mất hiệu lực.

Tham khảo chính thức: [tạo bot qua BotFather](https://core.telegram.org/bots/tutorial#obtain-your-bot-token), [Telegram Bot API](https://core.telegram.org/bots/api).
