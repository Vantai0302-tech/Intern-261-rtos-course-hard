# W2.2 – REPORT

## Số đo
- Log stress test `W2E2_STRESS_COUNT` phần tử: `received=1000000 errors=0`; số lỗi mất/lặp phát hiện = **0**.
- Bảng cycle:

| Cơ chế | Cycle trung bình (push/send) | Cycle trung bình (pop/receive) |
|---|---|---|
| SPSC ring buffer tự viết | 122 | 150 |
| `xQueueSendFromISR` / `xQueueReceive` | 500 | 664 |

Mỗi cơ chế đo 2019 lần thành công. Tổng trung bình: ring 272 cycles/cặp, queue 1164 cycles/cặp; tỉ số queue/ring là khoảng 4,28.

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- PREDICTION.md dự đoán ring 35 + 30 = 65 cycles/cặp và queue 250 cycles/cặp. Số đo lần lượt là 272 (cao hơn 207) và 1164 (cao hơn 914) cycles/cặp; tỉ số đo được 4,28 so với dự đoán 3,85. Cả bốn phép đo đều gồm chi phí đọc DWT và chạy ở `-O0`. Queue còn phải sao chép `spsc_elem_t` 4 byte, quản lý trạng thái queue và vùng bảo vệ nội bộ; phép đo hiện tại không tách riêng cycle của từng thao tác đó. `portYIELD_FROM_ISR` được gọi sau khoảng đo send, còn receive dùng timeout 0 nên không tính thời gian chờ dữ liệu.
- Stress test nhận đủ 1.000.000 phần tử và không phát hiện lỗi mất/lặp; không có `seq` sai đầu tiên để định vị.

## Trả lời câu hỏi trade-off
- Trên Cortex-M3 đơn nhân, chỉ TIM3 ISR ghi `head` và chỉ consumer task ghi `tail`; các lần đọc/ghi chỉ số 32 bit được căn chỉnh nên không bị chia ra. Producer ghi `storage[head & RING_MASK]`, gọi `__DMB()` rồi mới công bố `head + 1`; consumer đọc `head`, gọi `__DMB()` trước khi đọc phần tử, sau đó gọi `__DMB()` trước khi công bố `tail + 1`. Vì vậy consumer không đọc một slot trước khi producer công bố dữ liệu, còn producer không ghi đè slot trước khi consumer nhả nó. Khi ring đầy, ISR thử lại cùng `seq` ở lần ngắt sau. Phép thử thực tế nhận đủ 1.000.000 phần tử với `errors=0`; đây là bằng chứng cho cấu hình một producer, một consumer đã chạy, không phải chứng minh cho mọi cấu hình khác.

- Nếu thêm ISR producer thứ hai có thể ngắt producer thứ nhất, cả hai có thể cùng đọc `head = k`. ISR thứ hai ghi một phần tử và đặt `head = k + 1`; ISR thứ nhất chạy tiếp rồi cũng ghi vào slot `k` và đặt `head = k + 1`. Một phần tử bị mất. `__DMB()` chỉ bảo đảm thứ tự truy cập bộ nhớ, không biến cặp thao tác đọc–tăng–ghi `head` thành thao tác nguyên tử; trường hợp nhiều producer cần cơ chế đồng bộ khác. Vì vậy không suy rộng kết quả `errors=0` của phép thử SPSC sang trường hợp này.
