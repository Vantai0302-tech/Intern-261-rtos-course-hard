# W2.1 – REPORT

## Số đo
- Ảnh logic analyzer: trình tự EV5–EV8 bình thường (1 transaction write + 1 transaction read).
- Ảnh logic analyzer: SDA được PA1 kéo thấp → toggle SCL → PA1 nhả SDA → START/STOP → phục hồi.
- Bảng số lần phục hồi liên tiếp:

| Lần thử | Kết quả (OK / timeout / recovered) | Số xung SCL cần toggle |
|---|---|---:|
| 1 | recovered; OK | 1 |
| 2 | recovered; OK | 1 |
| 3 | recovered; OK | 1 |
| 4 | recovered; OK | 1 |
| 5 | recovered; OK | 1 |
| 6 | recovered; OK | 1 |
| 7 | recovered; OK | 1 |
| 8 | recovered; OK | 1 |
| 9 | recovered; OK | 1 |
| 10 | recovered  OK | 1 |
| 11 | recovered; OK | 1 |

Tổng số lần phục hồi liên tiếp thành công: **11 / 11** (`W2E1_RECOVERY_REPEAT_COUNT` trong PARAMS.md).

Phép thử dùng PA1 mô phỏng thiết bị giữ SDA thấp và nhả SDA ở xung SCL đầu tiên.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- PREDICTION.md nêu 9 xung SCL cho trường hợp xấu nhất. Cả 11 lần thử đều đếm được 1 xung, ít hơn mức tối đa. Trong phép thử này, PA1 mô phỏng thiết bị giữ SDA thấp và được code nhả ở cạnh lên SCL đầu tiên. Ảnh logic analyzer cũng cho thấy 1 xung ở lần đã chụp.
- `fault_to_return_cycles` nằm trong khoảng 2.595.936–2.596.744 cycles, tương ứng 36,055–36,066 ms ở 72 MHz. Khoảng này bắt đầu trước khi kéo SDA thấp và bao gồm `vTaskDelay(1 ms)` cùng timeout 35 ms; riêng timeout chiếm xấp xỉ 97% thời gian. `first_pulse_to_next_cycles = 25.841` cycles ở cả 11 lần, tương ứng 359 µs từ cạnh lên SCL phục hồi đầu tiên đến khi transaction kế tiếp hoàn tất. Dự đoán 1–5 ms bắt đầu từ lúc phát hiện bus-stuck; code chưa ghi timestamp đúng mốc đó, nên không so sánh trực tiếp hai khoảng thời gian. 
- Cả 11 lần đều `status=4` và transaction ghi kế tiếp `status=0`; không có lần thất bại cần định vị bằng tracer W1.2.

## Trả lời câu hỏi trade-off
- Khi SDA bị giữ thấp, I2C1 thấy bus bận và không thể dùng cách thông thường để tạo các xung SCL phục hồi. Driver tắt I2C1, chuyển các chân PB6/PB7 sang GPIO open-drain để chủ động kéo/thả SCL, rồi tạo STOP bằng cạnh lên SDA khi SCL cao trước khi trả hai chân về chức năng I2C.

- Trong phép thử PA1, logic analyzer cho thấy 1 xung SCL, SDA được nhả và có cạnh STOP. UART ghi 11/11 lần `status=4`; cả 11 transaction ghi tiếp theo đều `status=0`. Thời gian từ xung SCL phục hồi đầu tiên đến khi transaction kế tiếp hoàn tất là 25.841 cycles (khoảng 359 µs ở 72 MHz). 

- Nếu bỏ STOP, slave có thể còn ở trạng thái bị ngắt, khiến giao dịch tiếp theo bị hiểu sai hoặc báo lỗi. Đây là hệ quả cần kiểm chứng.