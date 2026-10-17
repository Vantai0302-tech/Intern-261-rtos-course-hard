# W1.2 – REPORT

**Trạng thái: w1e2 (-O0) và w1e2_o2 (-O2) build sạch; chưa đo trên board.**
Không có Gantt/ảnh LA được tạo giả. Thu UART 115200 8N1; reset sau khi mở ghi log. Dùng tools/trace_gantt.py của đề để vẽ Gantt.

## Định dạng và phạm vi đo

CSV `cycles,tag,IN|OUT`, tương thích nguyên bản tools/trace_gantt.py.
Dòng `#` là metadata. Timestamp tương đối với đầu capture, unsigned subtraction
xử lý một lần wrap; mỗi file Gantt chỉ chứa một capture ngắn hơn 59.65 giây.
Ring 512 records tĩnh, không overwrite; đóng ở OUT, UART chỉ xuất sau freeze.
Tag 1/2/3 map PB12/13/14 qua application-task-tag function; tag0 idle/monitor.

OUT->IN là khoảng giữa hook: bao gồm ghi ring ở OUT và phần scheduler trước IN;
không gồm toàn bộ save/restore/exception entry/exit. GPIO đánh dấu task được chọn,
bao gồm thời gian ISR. Không diễn giải kết quả thành CPU execution thuần.

## Số đo
- Gantt từ tracer (đính kèm ảnh xuất bởi `tools/trace_gantt.py`) và ảnh logic analyzer tương ứng.
- Bảng context switch:

| Optimization | min (cycle) | max (cycle) | jitter = max-min |
|---|---|---|---|
| -O0 | | | |
| -O2 | | | |

Chưa có số đo: min/max/jitter và verdict để trống có chủ ý.
Ngưỡng: LA mismatch<=13 us; O2 jitter<=309 cycle (~4.292 us).
Đọc timestamp ngay sau ghi GPIO giúp giảm lệch; độ trễ bus và ISR vẫn cần kiểm chứng.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- Sai lệch Gantt vs logic analyzer: ___ us, so với ngưỡng `W1E2_CORRELATION_TOLERANCE_US` trong PARAMS.md.
- Giải thích chênh lệch `-O0` vs `-O2` bằng assembly thực tế (đính kèm đoạn disassembly nếu cần).
- So `W1E2_CTXSW_JITTER_MAX_CYCLES_O2` (PARAMS.md) với jitter đo được ở `-O2`: đạt/không đạt, vì sao.

## Trả lời câu hỏi trade-off
Nguồn sai số nào (tick resolution, overhead ghi ring buffer, GPIO toggle) ảnh hưởng tracer nhiều nhất? Chứng minh bằng số đo của bạn.
