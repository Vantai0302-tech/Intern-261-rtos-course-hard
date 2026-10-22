# W1.2 – REPORT

## Số đo
- Gantt từ tracer (đính kèm ảnh xuất bởi `tools/trace_gantt.py`) và ảnh logic analyzer tương ứng.
- Bảng context switch:

| Optimization | min (cycle) | max (cycle) | jitter = max-min |
|---|---|---|---|
| -O0 |627|798|171|
| -O2 |323|404|81 |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- Sai lệch Gantt so với logic analyzer sau khi căn pha: 3,64 µs (-O0) và 4,10 µs (-O2), đều dưới ngưỡng `W1E2_CORRELATION_TOLERANCE_US` = 13 µs. Hai file chưa có mốc thu chung nên chưa thể xem đây là sai lệch của cùng một lần đo.
- Khoảng OUT → IN trung bình: 716,3 cycles (-O0) và 362,5 cycles (-O2), giảm 353,8 cycles, tức khoảng 4,91 µs (49,4%). Trong disassembly, bản -O0 dùng stack (`r7`) để lưu và đọc lại nhiều giá trị; bản -O2 giữ nhiều giá trị trong thanh ghi. Vì vậy thời gian đo ở -O2 ngắn hơn.
- Jitter -O2 = 404 - 323 = 81 cycles, thấp hơn ngưỡng `W1E2_CTXSW_JITTER_MAX_CYCLES_O2` = 309 cycles: đạt.

## Trả lời câu hỏi trade-off
- Tick 1 kHz quyết định lúc task được đánh thức, không quyết định độ chính xác timestamp DWT (72 MHz). Vì vậy độ phân giải tick 1 ms không phải sai lệch giữa Gantt và cạnh GPIO.
- GPIO được toggle trước khi đọc DWT nên có thể làm lệch thời điểm ghi; ring buffer được ghi sau timestamp nên chỉ kéo dài hook. Sai lệch sau căn pha là 3,64 µs (-O0) và 4,10 µs (-O2). 