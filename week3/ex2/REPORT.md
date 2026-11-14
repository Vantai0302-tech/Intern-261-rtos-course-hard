# W3.2 – REPORT

## Số đo

- Bảng dòng tiêu thụ (chưa đo):

| Trạng thái | Dòng đo (mA / µA) |
|---|---|
| Active | |
| Idle (STOP mode) | |

- Bảng trôi tick trong `W3E2_DRIFT_TEST_WINDOW_MIN = 10` phút. Hai cột thời gian tính từ mốc UART đầu tiên; thời gian PC lấy bằng đồng hồ monotonic trong `log_drift.py`.

| Giờ MCU (ms) | Giờ PC đã trôi (ms) | Chênh lệch (ms) |
|---|---|---|
| 0 | 0,000 | 0,000 |
| 60000 | 60000,581 | -0,581 |
| 120000 | 120000,506 | -0,506 |
| 180000 | 180000,280 | -0,280 |
| 240000 | 240000,041 | -0,041 |
| 300000 | 299999,838 | +0,162 |
| 360000 | 359999,747 | +0,253 |
| 420000 | 419999,557 | +0,443 |
| 480000 | 479999,064 | +0,936 |
| 540000 | 539998,553 | +1,447 |
| 600000 | 599997,518 | +2,482 |

- Log UART đính kèm trong [ex2w3_drift.txt](ex2w3_drift.txt). Chênh lệch tính bằng thời gian MCU trừ thời gian PC, nên giá trị dương cho biết MCU chạy nhanh hơn PC theo phép đo.
- Ảnh logic analyzer [w3e2_stop.png](w3e2_stop.png): D0 nối PA1 đánh dấu khoảng STOP gần 4 s; D2 nối PA2 đánh dấu busy-wait khoảng 1 s. Chu kỳ quan sát khoảng 5 s, đọc gần đúng từ thước thời gian của ảnh.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- Sau 10 phút, độ trôi đo được là +2,482 ms. Sai lệch bằng 2,482 / 599997,518 × 1000000 ≈ 4,137 ppm, nhỏ hơn `W3E2_DRIFT_BOUND_PPM = 65` (khoảng 39 ms trong 600 s). Lần đo UART so với PC đạt ngưỡng này.
- PREDICTION.md giả định LSE ±20 ppm, tương ứng khoảng ±12 ms trong 10 phút. Độ trôi đo được nhỏ hơn mức đó, nhưng dung sai thực của thạch anh vẫn cần xác nhận theo mã linh kiện.
- Counter RTC chạy 32768 Hz, mỗi số đếm khoảng 30,518 us. Code giữ phần dư khi quy đổi và đặt alarm sớm khoảng 2 ms để khôi phục clock. Khoảng 2 ms là giá trị cấu hình, chưa phải số đo thời gian vào/ra STOP.
- Hiện chưa tách được sai số oscillator, làm tròn bù tick và thời gian vào/ra STOP để cộng riêng từng phần. Phép đo còn có độ trễ USB-UART và xử lý trên PC, nên 2,482 ms là độ lệch tổng quan sát được giữa MCU và PC.
- So sánh dòng tiêu thụ với PREDICTION.md: chờ số đo thực tế.

## Trả lời câu hỏi trade-off

- Ngủ lâu hơn làm giảm số lần vào/ra STOP, có thể giảm overhead và sai số phát sinh sau mỗi lần chuyển chế độ. Sai số tần số RTC vẫn tích lũy theo thời gian; độ phân giải counter vẫn khoảng 30,518 us khi giữ nguyên clock RTC.
- Alarm phải được đặt kịp thời hạn đánh thức task. Nếu kéo dài thời gian ngủ vượt deadline, task sẽ phản ứng trễ; các ngắt đánh thức khác vẫn có thể làm MCU thức sớm.
- Lần thử hiện tại dùng chu kỳ khoảng 1 s active và 4 s STOP, đo được độ trôi khoảng 4,137 ppm. Chưa có lần đo với khoảng ngủ dài hơn để so sánh định lượng sự đánh đổi.
