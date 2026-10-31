# W1.3 – REPORT

## WCRT đo được (từ tracer W1.2)

Log: [w1e3_uart1.txt](w1e3_uart1.txt). Lịch chạy: [gantt_rta.svg](gantt_rta.svg).
Clock CPU 72 MHz; WCRT (ms) = max_cycles / 72.000. Thời gian thu khoảng 98,7 giây, hơn 82 hyperperiod (mỗi hyperperiod 1.200 ms).

| Task | RTA lý thuyết (ms) | WCRT đo được (ms) | Chênh lệch | % |
|---|---|---|---|---|
| τ0, tag 1 | 7,485 | 7,549 | +0,064 ms | +0,862% |
| τ1, tag 2 | 17,514 | 17,651 | +0,137 ms | +0,784% |
| τ2, tag 3 | 36,802 | 37,049 | +0,247 ms | +0,670% |
| τ3, tag 4 | 64,955 | 65,394 | +0,439 ms | +0,677% |

Số job hoàn tất lần lượt là 2.469, 1.235, 823 và 659. Các WCRT đo được đều nhỏ hơn deadline tương ứng 40, 80, 120 và 150 ms.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Trước scheduler, DWT hiệu chỉnh số vòng lặp tạo tải. Sai lệch giữa target_cycles và measured_cycles của bốn task lần lượt là -6, +6, -2 và -2 cycles; lớn nhất 0,083 µs. Khi task bị ngắt, vòng lặp tạm dừng rồi chạy tiếp nên thời gian task khác chạy không làm giảm lượng công việc của job.

Xét một cửa sổ 1.200 ms trong trace, từ cycle 2.718.200.579 đến trước cycle 2.804.600.579: có 96 cặp OUT→IN, tổng khoảng cách 67.361 cycles = 0,936 ms. Trung bình là 701,7 cycles (9,745 µs), min 607 và max 775 cycles. W1.2 ở -O0 đo trung bình 716,3 cycles (9,949 µs); nếu dùng số đó cho 96 lần chuyển thì được khoảng 0,955 ms, cao hơn tổng khoảng cách quan sát ở W1.3 khoảng 0,019 ms.

Trong cửa sổ trên, có 6 lần tag 4 bị chuyển sang tag có ưu tiên cao hơn; các task còn lại không có lần chuyển trực tiếp như vậy. Ước tính hai lần chuyển cho mỗi lần ngắt rồi tiếp tục chạy: 6 × 2 × 716,3 = 8.595,6 cycles, tương đương 0,119 ms cho toàn cửa sổ. Đây là phần của tổng thời gian chuyển task, không cộng thêm vào tổng 0,936 ms.

WCRT được tích lũy qua hơn 82 hyperperiod, còn ring buffer chỉ giữ đoạn lịch cuối. Vì vậy không biết chắc job đạt WCRT lớn nhất có nằm trong cửa sổ trên hay không. Các khoảng OUT→IN cũng không đo toàn bộ chi phí SysTick, đánh thức task và thực thi hook. Số liệu hiện tại cho thấy chi phí chuyển task có ảnh hưởng, nhưng chưa đủ để phân bổ chính xác mức lệch +0,064 đến +0,439 ms cho từng nguồn. Không quy toàn bộ mức lệch này cho jitter chuyển task.

## Nếu task set không schedulable

Bộ tham số hiện tại schedulable theo RTA: bốn R hội tụ đều nhỏ hơn deadline. Trong khoảng thời gian đã đo, WCRT cũng đều nhỏ hơn deadline nên chưa cần thay T, C hoặc priority.

## Trả lời câu hỏi trade-off

Giới hạn Liu–Layland là điều kiện đủ cho bộ task theo giả định RM cổ điển: nếu utilization không vượt giới hạn thì có thể kết luận schedulable; nếu vượt thì chưa thể kết luận không schedulable. Với bốn task của tôi, U = 0,611008 < 4(2^(1/4) - 1) ≈ 0,7568 nên đạt điều kiện này.

RTA xét riêng từng task và phần công việc của các task ưu tiên cao hơn. Trong mô hình task độc lập, lập lịch ưu tiên cố định có preemption và deadline bằng period đang xét, các R hội tụ 7,485; 17,514; 36,802; 64,955 ms đều nhỏ hơn deadline. RTA cung cấp cận response time cụ thể để đối chiếu với phép đo, còn giới hạn Liu–Layland chỉ cho kết luận đủ về tính lập lịch được. Phần overhead thực tế chưa nằm trong C lý thuyết cần được giải thích riêng bằng số đo.
