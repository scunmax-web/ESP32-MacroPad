# ESP32 Macro Pad 8 V1.0 — GitHub Actions

1. Tạo repository GitHub mới (Private được), đặt tên ESP32-MacroPad8.
2. Upload thư mục ESP32_MACRO_PAD_8_V1_0 và các file README.md, web_app.html, HUONG_DAN.txt ở gốc repository. Commit.
3. Tại repository chọn Add file > Create new file. Nhập tên `.github/workflows/build.yml`, dán toàn bộ nội dung file build.yml trong gói này, Commit. (Nếu đã tải thư mục .github lên thì bỏ qua.)
4. Chọn Actions > Build Macro Pad 8 BIN. Nếu chưa tự chạy, chọn Run workflow > Run workflow. Nếu Actions bị tắt, bật trong Settings > Actions.
5. Khi workflow xanh, mở lần chạy > Artifacts > MacroPad8-ESPBoards-BIN. Tải ZIP và giải nén lấy ESP32_MACRO_PAD_8_V1_0_MERGED.bin.
6. Mở https://www.espboards.dev/tools/program/ bằng Chrome/Edge, kết nối USB, chọn ESP32-C3, chọn BIN và offset 0x0. Không chọn xóa toàn bộ flash nếu cần giữ cấu hình cũ. Lần đầu nếu không kết nối được: giữ BOOT, bấm nhả RESET (hoặc cắm lại USB), nhả BOOT, chọn lại cổng. Sau nạp nhấn RESET.

Nguồn firmware giữ nguyên V1.0. Workflow dùng Arduino CLI trên máy chủ, không cần cài Arduino IDE trên PC. Chưa chạy build thật trên GitHub, chưa kiểm thử mạch. Nếu lỗi, mở bước màu đỏ, sao chép log.

GPIO nút 1–8: 0,1,3,4,5,6,7,10; chân còn lại nối GND. Giữ BOOT 3 giây khi đang chạy để bật Wi-Fi ESP32-MacroPad, mật khẩu Macro1234, truy cập http://192.168.4.1. Ghép đôi Bluetooth Macro Pad 8 trong Windows.
