# W4.2 – REPORT

## Số đo
- Bảng stack high-water-mark thực đo mỗi task (bytes/words) so với ngân sách đã khai ở W4.1:

| Task | Stack budget (W4.1) | High-water-mark đo được | Chênh lệch |
|---|---|---|---|
| control | 256 words | 203 words trống / 256 words cấp | 0 words |
| imu | 320 words | 303 words trống / 384 words cấp | +64 words |
| telemetry | 256 words | 217 words trống / 384 words cấp | +128 words |
| shell | 256 words | 252 words trống / 384 words cấp | +128 words |
| idle_aux | 192 words | 148 words trống / 192 words cấp | 0 words |

Mỗi word bằng 4 bytes. Ngân sách lấy từ [DESIGN.md W4.1](../ex1/DESIGN.md); chênh lệch trong bảng là stack cấp thực tế trừ ngân sách thiết kế. High-water-mark là số word trống nhỏ nhất, không phải số word đã dùng.

- CPU load đo được (run-time stats) khi hệ thống chạy ổn định: **9,99%** ở mẫu UART đã thu, `telemetry_ms=39`. UART thực tế **921600 baud** với CP2102, khác thông số đề **1000000 baud**.
- `tools/check_api.sh` kết quả: **PASS (8 files)**; target `w4e2` build sạch với `-Wall -Wextra -Werror`.
- Bằng chứng pause → STOP → resume: [ảnh tổng quan](w4e2-image3.png), [ảnh chi tiết](w4e2_stop_detail.png), [CSV](w4e2_stop.csv). Lần thu 10 s có 125 khoảng PA1 mức cao, tổng khoảng 5,865 s; mỗi khoảng dài 9,46–76,87 ms, trung bình 46,92 ms. Giữa hai khoảng cao, PA1 thấp 2,12–2,16 ms. PA2 không có xung control trong đoạn khoảng 2,931–9,060 s và xuất hiện lại sau đó. Phép thử STOP thực hiện khi control đã tạm dừng theo phương án Ex1.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- Mức dùng stack lớn nhất tính bằng số word cấp trừ high-water-mark: control **53**, imu **81**, telemetry **167**, shell **132**, idle_aux **44 words**. Idle_aux còn ít word trống nhất, khớp dự đoán. Ba task IMU/telemetry/shell đang được cấp nhiều stack hơn bảng Ex1 như trên; chưa kết luận đạt dung sai 11 words.
- CPU quan sát **9,99%**, thấp hơn dự đoán **12%** là **2,01 điểm phần trăm**. Chưa đo tách từng task/ISR để xác định phần đóng góp vào chênh lệch.
- Quan sát được STOP và control chạy lại sau pause. Khoảng PA1 thấp giữa các lần STOP gồm hoạt động khi thức, không phải riêng wake-up latency. CSV STOP lấy mẫu khoảng 10 us nên chưa dùng để kết luận jitter control chính xác.

## Trả lời câu hỏi trade-off
Vì sao control task dùng TIM + notification thay vì `vTaskDelay` theo tick? Chứng minh bằng chính số đo jitter
của bạn (so với thiết kế jitter bound ở `W4E1_CONTROL_LOOP_JITTER_MAX_US`).

TIM compare tạo mốc kích mỗi 1000 us, ISR gửi notification để control chạy và block khi hết việc. `vTaskDelay` phụ thuộc tick 1 ms và thời điểm gọi hàm, nên không giữ mốc kích độc lập với thời gian xử lý như TIM.

Hai lần thu [CSV 1](w4e2-1.csv) và [CSV 2](w4e2-2.csv), lấy mẫu danh định 1 MHz, đều có 1000 xung PA2 trong 1 s. Khoảng cách hai cạnh lên liên tiếp nhỏ nhất 989 us, lớn nhất 1011 us: lệch tối đa quan sát khoảng 11 us so với chu kỳ 1000 us, biên max-min là 22 us.

Lần thu [CSV TIM3 và control](w4e2_tradeoff.csv) ở 1 MHz có 1000 cặp cạnh PA6 → cạnh lên PA2. Độ trễ từ TIM compare đến khi control bắt đầu nhỏ nhất **13 us**, lớn nhất **36 us**, trung bình **19,72 us**; biên max-min là **23 us**. Độ trễ lớn nhất thấp hơn ngưỡng **109 us** là **73 us**, đạt giới hạn trong lần thu này. Độ phân giải lấy mẫu khoảng 1 us.

