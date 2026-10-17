# W1.1 – REPORT

**Trạng thái: code đã build/kiểm tra ELF trên PC; chưa nạp board hoặc đo logic analyzer.**
MSSV 2413032. Các ô `___` dưới đây chờ bằng chứng thật, không được điền bằng dự đoán.

Triển khai: root CMake tạo w1e1/w1e1_flash; startup/linker/clock/GPIO/UART tự viết;
FreeRTOS GCC/ARM_CM3. Hai task priority1, xTaskDelayUntil, PB0 theo params,
PA1/PA2 giúp đo riêng task. TIM2_CH1 ở PA0. Xem SETUP.md.

## Số đo
- Tần số TIM đo được (ảnh logic analyzer đính kèm): ___ Hz (dự đoán: xem PREDICTION.md).
- Sai lệch so với dự đoán (Hz và %): ___
- Chu kỳ blink đo thực tế của task A/B: ___ ms / ___ ms.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
So sánh số đo với PREDICTION.md; nếu lệch, chỉ ra nguồn gốc (sai số HSE crystal, làm tròn PLL, độ phân giải logic analyzer...) kèm con số.

Tính ppm=(f_measured-1000)/1000*1e6; yêu cầu |ppm|<=282.
Với blink, đo khoảng cách hai cạnh liên tiếp PA1/PA2; chu kỳ sóng đầy đủ gấp đôi.
Hai task cùng toggle PB0 nên không dùng chu kỳ PB0 để kết luận riêng task A/B.

## Giải thích thanh ghi (tham chiếu SETUP.md)
Tóm tắt 3 thanh ghi quan trọng nhất đã cấu hình và vì sao.

## Trả lời câu hỏi trade-off
Vì sao PendSV phải có priority thấp nhất trong toàn hệ thống? Trả lời bằng chính traceback/chứng cứ đo của bạn.
