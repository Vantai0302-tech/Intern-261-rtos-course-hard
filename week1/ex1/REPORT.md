# W1.1 – REPORT

**Trạng thái: đã thu dữ liệu logic analyzer trên board STM32 mới; ảnh chụp của lần đo này chưa đính kèm.**
MSSV 2413032. Các số đo dưới đây lấy từ `ex1-5.csv` (PulseView, 20 kHz, khoảng 34,227 s).

Triển khai: root CMake tạo w1e1/w1e1_flash; startup/linker/clock/GPIO/UART tự viết;
FreeRTOS GCC/ARM_CM3. Hai task priority1, xTaskDelayUntil, PB0 theo params,
PA1/PA2 giúp đo riêng task. TIM2_CH1 ở PA0. Xem SETUP.md.

## Số đo
- Ánh xạ kênh đo: CH1/PA0 (TIM2), CH3/PB0 (chung), CH5/PA1 (task A), CH7/PA2 (task B).
- Tần số TIM đo được trên PA0: **999,976627 Hz** (dự đoán 1000 Hz).
- Sai lệch: **−0,023373 Hz = −0,0023373% = −23,373 ppm**.
- Khoảng cách hai cạnh liên tiếp của task A/B: **379,008889 ms / 711,015957 ms**. 
- PB0 có 139 cạnh, bằng tổng 91 cạnh PA1 và 48 cạnh PA2; 136 cạnh trùng thời điểm lấy mẫu, 3 cạnh lệch một mẫu 50 µs.
- Ảnh PulseView:

## Giải thích sai lệch (ĐỊNH LƯỢNG)
Theo PREDICTION.md, khoảng cho phép của xung TIM là 999,718–1000,282 Hz. Phép đo 999,976627 Hz nằm trong khoảng này:
`ppm = (999,976627 − 1000) / 1000 × 10^6 = −23,373`.

So với tham số 379/711 ms, khoảng toggle của task A dài hơn **0,008889 ms** (khoảng +23,45 ppm), task B dài hơn **0,015957 ms** (khoảng +22,44 ppm). Sai lệch thời gian cùng cỡ và ngược dấu với sai lệch tần số TIM. PLL ×9 và bộ chia TIM2 là số nguyên, nên không có sai số do làm tròn các hệ số này. Độ phân giải lấy mẫu là 50 µs; việc lấy trung bình trên nhiều chu kỳ giảm sai số xác định hai đầu khoảng đo, nhưng chuẩn thời gian tuyệt đối của logic analyzer chưa được kiểm định độc lập. Vì vậy chưa thể quy sai lệch −23,373 ppm riêng cho HSE hay máy đo.



## Giải thích thanh ghi (tham chiếu SETUP.md)
- FLASH_ACR – Cấu hình Flash latency: Thiết lập LATENCY = 2 wait-state và bật prefetch trước khi tăng SYSCLK lên 72 MHz. Điều này bảo đảm CPU có thể đọc lệnh từ Flash ổn định ở tần số cao, tránh lỗi thực thi chương trình.

- RCC_CFGR – Cấu hình System Clock: Chọn HSE 8 MHz làm nguồn cho PLL, cấu hình PLL nhân 9 để tạo SYSCLK = 72 MHz. Đồng thời thiết lập APB1 chia 2 và APB2 không chia. Đây là cơ sở tạo clock cho CPU, FreeRTOS và TIM2.

- SCB_SHPR3 – Cấu hình ưu tiên Exception: Thiết lập PendSV và SysTick ở logical priority 15 (giá trị mã hóa 0xF0), là mức ưu tiên thấp nhất của Cortex-M3 khi sử dụng 4 bit priority. Điều này giúp PendSV thực hiện context switch sau khi các interrupt có ưu tiên cao hơn hoàn tất, bảo đảm cơ chế lập lịch của FreeRTOS hoạt động đúng.

## Trả lời câu hỏi trade-off
Trong FreeRTOS trên Cortex-M3, PendSV đảm nhiệm chuyển ngữ cảnh giữa các task. Đặt PendSV ở mức ưu tiên thấp nhất giúp việc chuyển ngữ cảnh chờ các ISR có ưu tiên cao hơn hoàn thành.Trong chương trình, PendSV được cấu hình ở logical priority 15 (giá trị mã hóa 0xF0), là mức thấp nhất khi configPRIO_BITS = 4.Chứng cứ thực nghiệm: Logic Analyzer đo được TIM2 gần 1 kHz (999.9766 Hz), Task A toggle mỗi 379.009 ms và Task B mỗi 711.016 ms. Hai task hoạt động định kỳ gần với giá trị dự đoán, cho thấy scheduler FreeRTOS đang vận hành ổn định trong thử nghiệm.
