# W3.1 – REPORT

## Số đo

- Log 10 phút `tools/uart_stress.py` (đính kèm file/CSV): tổng khung gửi = 5408, OK = 5408, lỗi CRC = 0, mất = 0.
- Bảng CPU load (run-time stats) lấy mẫu trong lúc stress test:

| Thời điểm (uptime MCU, s) | CPU load (%) |
|---|---|
| 60,092 | 1,81 |
| 120,188 | 1,81 |
| 180,284 | 1,81 |
| 240,381 | 1,78 |
| 300,483 | 1,81 |
| 360,579 | 1,81 |
| 420,675 | 1,82 |
| 480,773 | 1,82 |
| 540,871 | 1,82 |
| 600,969 | 1,82 |

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- DMA buffer 256 byte và stream buffer 512 byte đúng với PREDICTION.md. Stream buffer dùng cao nhất 54 byte, khoảng 10,55% dung lượng; số byte bị bỏ do đầy buffer và các bộ đếm lỗi DMA/UART đều bằng 0.
- Gửi và nhận đủ 5408 khung, không mất SEQ và không lỗi CRC. MCU nhận 292032 byte = 5408 × 54, khớp log PC.
- Tốc độ thực tế khoảng 9,013 khung/s, thấp hơn mức 26 khung/s khoảng 65,33%. Script đọc `ser.read(4096)` với timeout 0,1 s sau mỗi lần gửi, làm chậm nhịp gửi. Cần đo lại đúng 26 khung/s để xác nhận đầy đủ yêu cầu.
- CPU load trung bình 1,82% trên 119 mẫu trong lúc stress, dao động 1,78–1,93%, thấp hơn dự đoán 5% khoảng 3,18 điểm phần trăm. Tải gửi thực tế thấp hơn dự kiến góp phần làm giảm CPU load. Số đo dùng TIM4 riêng; chưa tách chi phí ISR và parser để xác định đóng góp từng phần.

## Trả lời câu hỏi trade-off
- Với UART 460800 bps, 8N1, tốc độ truyền liên tục tối đa là 46080 byte/s. DMA buffer 256 byte có mỗi nửa 128 byte, tương ứng khoảng 2,778 ms trước khi DMA quay lại vùng vừa hoàn thành nếu dữ liệu đến liên tục. Chọn kích thước nhỏ hơn sẽ rút ngắn thời gian phục vụ ngắt và chuyển dữ liệu trước khi bị ghi đè.
- Stream buffer 512 byte chứa được 9 khung đầy đủ, mỗi khung 54 byte. Ở tải dự kiến 1404 byte/s, dung lượng này tương ứng khoảng 365 ms dữ liệu tồn đọng; nếu nhận liên tục ở mức 46080 byte/s thì chỉ tương ứng khoảng 11,11 ms. Buffer này tạo khoảng đệm khi parser chưa được chạy kịp.
- Hai vùng dữ liệu hiện dùng 256 + 513 = 769 byte RAM, gồm một byte dự phòng theo cách cấp vùng nhớ cho static stream buffer. Nếu tăng gấp đôi dung lượng DMA và stream buffer lên 512 và 1024 byte thì cần thêm 768 byte RAM, chưa tính các cấu trúc quản lý. Trong lần đo này, stream buffer dùng cao nhất 54 byte, không bỏ dữ liệu và CPU load khoảng 1,82%, nên chưa có số đo cho thấy cần tăng buffer.




