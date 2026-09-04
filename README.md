# SmartHome ESP32 IoT Gateway

## Tổng quan (Overview)
SmartHome ESP32 IoT Gateway là dự án IoT tích hợp giao thức MQTT đảm bảo truyền thông an toàn, được thiết kế theo cấu trúc module giúp dễ dàng mở rộng và nâng cấp. Dự án sử dụng PlatformIO làm nền tảng quản lý mã nguồn và xây dựng hệ thống.

## Hướng dẫn bắt đầu (Getting Started)

1. **Clone Repository**: Tải mã nguồn về máy tính cục bộ từ kho lưu trữ GitHub của nhóm.

2. **Cài đặt Wokwi Simulator Extension**: 
   - Mở VS Code, truy cập vào mục **Extensions** (phím tắt `Ctrl + Shift + X`).
   - Tìm kiếm từ khóa **"Wokwi Simulator"** và bấm **Install** để cài đặt tiện ích mở rộng giúp chạy mô phỏng mạch điện tử.

3. **Build dự án lần đầu**: Mở thư mục dự án trong VS Code và tiến hành Build bằng PlatformIO. Thao tác này sẽ kích hoạt script `auto_secrets.py` tự động sinh file `include/secrets.h`. 
   *(Hoặc bạn có thể tự sao chép file `include/secrets.example.h` thành `include/secrets.h`).*

4. **Cấu hình thông tin (Secrets)**: Mở file `include/secrets.h` vừa tạo và điền thông tin mạng Wi-Fi cũng như MQTT Broker của bạn vào.

5. **Chạy mô phỏng trên Wokwi**: 
   - Mở file `diagram.json` tại thư mục gốc để kiểm tra sơ đồ mạch.
   - Nhấn phím `F1` (hoặc `Ctrl + Shift + P`), gõ và chọn lệnh **"Wokwi: Start Simulation"** để khởi động mô phỏng mạch ESP32.

## Tính năng nổi bật (Features)

- **Tự động sinh file mật khẩu**: Script `auto_secrets.py` tự động tạo file `include/secrets.h` chứa thông tin kết nối trong lần build đầu tiên, ngăn chặn việc vô tình đẩy thông tin nhạy cảm lên GitHub.
- **Cấu trúc Module hóa**: Dự án được phân chia rõ ràng thành các tầng riêng biệt: tầng cảm biến (`sensors.h` / `src/sensors.cpp`), tầng mạng (`mqtt_handler.h` / `src/mqtt_handler.cpp`), tầng xử lý biên (`access_control.h` / `src/access_control.cpp`) và tầng ứng dụng (`main.cpp`).
- **Quản lý thư viện tự động**: Sử dụng tính năng Library Dependency Finder của PlatformIO để tự động quét và nhận diện các thư viện phụ thuộc.

## Hướng phát triển tương lai (Future Improvements)

- **Mở rộng cảm biến**: Hỗ trợ thêm các loại cảm biến mới bên cạnh PIR, LDR, DHT22 và RFID hiện tại.
- **Cập nhật Firmware từ xa (OTA)**: Cho phép nâng cấp hệ thống qua mạng không dây mà không cần tiếp cận phần cứng trực tiếp.
- **Tăng cường bảo mật**: Bổ sung các lớp mã hóa chuyên sâu cho quá trình truyền tải dữ liệu qua MQTT.

## Giấy phép (License)
Dự án được phân phối dưới giấy phép MIT. Xem file `LICENSE` để biết thêm chi tiết.