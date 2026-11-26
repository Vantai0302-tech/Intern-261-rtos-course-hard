# W4.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán priority/stack
- Bạn dự đoán task nào sẽ có stack high-water-mark gần ngân sách nhất (margin nhỏ nhất): idle_aux, lý do: idle_aux chỉ được cấp phát 192 words (768 bytes), thấp nhất trong năm task ứng dụng. Task này còn thực hiện giám sát và phục hồi I2C bus, có thể sử dụng nhiều stack hơn trong nhánh lỗi do các lời gọi hàm lồng nhau và biến cục bộ..
- Dự đoán CPU load tổng khi hệ thống chạy ổn định (không fault): 12 %, so với `W4E1_CPU_BUDGET_PERCENT_MAX`.

## Dự đoán hành vi tickless
- Dự đoán hệ thống có vào STOP mode được không khi IMU/telemetry/shell đều rảnh: Khi Control vẫn hoạt động liên tục ở tần số 1 kHz, hệ thống không được phép vào STOP mode theo thiết kế W4.1 hiện tại, kể cả khi các task còn lại đang Blocked.

Nguyên nhân là STOP mode dừng clock của các TIM thông thường trên STM32F103. Việc đánh thức bằng RTC và khôi phục PLL có thể gây trễ deadline hoặc vượt giới hạn jitter của Control.

Trong trạng thái ACTIVE_CONTROL, hệ thống chỉ sử dụng Sleep/WFI nông khi rảnh.

Khi chuyển sang trạng thái SAFE_PAUSED, Control được tạm dừng an toàn và các giao dịch ngoại vi hoàn tất, hệ thống có thể sử dụng Tickless Idle kết hợp RTC Alarm để vào STOP mode.
- Dự đoán độ trễ thức dậy từ STOP mode có ảnh hưởng tới jitter control loop không, bao nhiêu us: Có khả năng ảnh hưởng nếu cho phép STOP khi Control đang hoạt động.

Trong W3.2, firmware sử dụng khoảng dự phòng 2 ms (2.000 µs) để đánh thức sớm và khôi phục clock. Đây là thời gian dự phòng cấu hình, không phải số đo wake-up latency thực tế.

Giới hạn jitter Control của W4.1 là 109 µs. Do chưa đo và chứng minh wake-to-control không vượt 109 µs, không thể bảo đảm Control giữ deadline khi thoát STOP.

## Rủi ro đã lường trước
- Nếu priority đã duyệt ở W4.1 bị vi phạm khi hiện thực (do nhầm lẫn): dự đoán triệu chứng quan sát được: control có thể bị task ít quan trọng hơn chiếm CPU, làm tăng jitter hoặc trễ deadline 1 ms. .
- Nếu `imu` không kịp cung cấp mẫu mới trước khi `control` cần: dự đoán hệ quả (dùng mẫu cũ, bỏ chu kỳ, hay gì khác): control dùng mẫu hợp lệ gần nhất, không chờ I2C; khi mẫu không hợp lệ hoặc quá 5 ms thì đưa đầu ra về 0 theo thiết kế..
