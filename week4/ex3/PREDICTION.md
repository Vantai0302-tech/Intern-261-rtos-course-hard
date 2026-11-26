# W4.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán WCRT/CPU/stack (chạy ổn định, chưa fault)
- Dự đoán task nào lệch WCRT đo được so với RTA ở W4.1 nhiều nhất, trong ngưỡng `W4E3_WCRT_TOLERANCE_PERCENT`: idle_aux, vì RTA trong DESIGN.md tính cả ngân sách recovery, còn khi chạy ổn định không có công việc này. WCRT đo được dự kiến thấp hơn RTA; chênh lệch có thể vượt 13%.
- Dự đoán CPU load tổng đo được: khoảng 12%.

## Dự đoán từng kịch bản fault (xem `W4E3_ASSIGNED_FAULT_NAMES` trong PARAMS.md)
Với mỗi kịch bản được giao, dự đoán TRƯỚC khi chạy:
- Kịch bản 1 (TASK_HANG):  treo idle_aux bằng vòng lặp vô hạn nhưng không tắt ngắt. Dự đoán control không trễ deadline vì có priority cao hơn. Watchdog sẽ reset khi bộ giám sát phát hiện task không còn tiến triển và ngừng nạp watchdog.
- Kịch bản 2 (UART_FLOOD): dự đoán control không trễ deadline nếu thời gian xử lý ngắt vẫn nằm trong ngân sách Ex1. Shell có thể bỏ dữ liệu và tăng bộ đếm tràn; đây là cách suy giảm đã nêu ở Ex1..
- Kịch bản 3 (SENSOR_UNPLUG): dự đoán control không trễ deadline. IMU báo mẫu không hợp lệ, control đặt đầu ra về 0 và idle_aux thử recovery. Đây là cách xử lý lỗi cảm biến đã nêu ở Ex1.
- (Nếu được giao 4 kịch bản) Kịch bản 4 (CPU_OVERLOAD): nếu task busy-wait có priority cao hơn control, dự đoán control sẽ trễ deadline. Trường hợp này chưa được công bố cụ thể trong thiết kế Ex1; cần bổ sung phương án phát hiện, trạng thái an toàn và watchdog reset trước khi thử.

## Rủi ro đã lường trước
- Nếu watchdog không phục hồi đúng `W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS`: dự đoán hệ quả quan sát được: hệ thống có thể tiếp tục treo hoặc mất điều khiển quá 302 ms. Dự kiến quan sát thấy task không tiến triển, không có thông báo khởi động lại hoặc hệ thống khởi động lại quá muộn.
