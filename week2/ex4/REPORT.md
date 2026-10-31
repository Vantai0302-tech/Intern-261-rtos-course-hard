# W2.4 – REPORT

## Số đo

Trên board STM32F103C8T6, `sizeof(StaticEventGroup_t) = 32` byte, `sizeof(StaticSemaphore_t) = 80` byte và `sizeof(StaticTask_t) = 96` byte. Sáu task barrier dùng 6 × 96 = 576 byte TCB và 6 × 192 × 4 = 4.608 byte stack. Cộng với event group, ba thành phần này chiếm 5.216 byte; chưa tính handle, biến đếm và các vùng tĩnh khác. Bảng dưới chỉ so RAM của đối tượng báo hiệu; TCB và stack của hai task gửi/nhận là phần dùng chung.

| Cơ chế | RAM đối tượng báo hiệu (bytes) | Gửi trung bình (cycles) | Nhận trung bình (cycles) | Tổng (cycles) |
|---|---:|---:|---:|---:|
| Binary semaphore | 80 | 537 | 405 | 942 |
| Notification `eIncrement` | 0 byte đối tượng riêng | 376 | 396 | 772 |

Benchmark chạy 1000 lượt cho mỗi cơ chế. DWT đo riêng lời gọi gửi và nhận khi sự kiện đã sẵn, không gồm UART, tracer hay thời gian task ngủ chờ. Notification giảm 170 cycles/lượt, tức 18,0% so với binary semaphore theo phép đo này. Event group 32 byte dùng cho barrier được tính riêng, không tính vào hai hàng so sánh báo hiệu trên.

Từ lúc task thứ sáu đến barrier đến lúc cả sáu task thoát khỏi `xEventGroupSync()`, DWT đo được 24.168 cycles = 335,7 µs. Con số này thấp hơn mức dự đoán 72.000 cycles (1.000 µs) là 47.832 cycles (664,3 µs), nên đạt dự đoán. Cả sáu task đã thoát trong giới hạn timeout 375 ms.

### Lỗi mất sự kiện

| Biến thể | Số lần gửi | Số sự kiện nhận được | Chênh lệch (mất) |
|---|---|---|---|
| `eSetValueWithOverwrite` (buggy) | 1000 | 500 | 500 |
| `eIncrement` (đã sửa) | 1000 | 1000 | 0 |

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Dự đoán mất khoảng 500 sự kiện trên 1000 lần gửi; số đo đúng bằng 500. Ở đầu trace, `SEND,1` tại cycle 1067913 và `SEND,2` tại 1068925, cách nhau 1012 cycles (14,1 µs). `RECEIVE,2` chỉ xuất hiện tại cycle 1072479, sau lần gửi thứ hai 3554 cycles (49,4 µs). Task nhận chưa chạy giữa hai lần gửi nên giá trị 1 bị `eSetValueWithOverwrite` ghi đè. Mỗi cặp gửi lặp lại theo cùng cách điều phối, dẫn đến 1000 lần gửi nhưng chỉ 500 lần nhận.

Với `eIncrement`, hai lần gửi trước một lần nhận làm notification value tăng thành 2. Task nhận cộng giá trị này vào tổng số sự kiện, nên log ghi nhận 1000 gửi và 1000 sự kiện nhận trong lần thử này. Dự đoán notification nhanh hơn semaphore khoảng 20%; phép đo API cho thấy giảm 18,0%, thấp hơn dự đoán 2,0 điểm phần trăm. Phép đo không bao gồm thời gian đánh thức task nên chưa thể suy ra chênh lệch độ trễ đầu cuối.

## Trả lời câu hỏi trade-off

Notification không cần đối tượng semaphore 80 byte riêng và dùng ít hơn 170 cycles/lượt trong phép đo API. Đổi lại, `eIncrement` chỉ giữ tổng số lần báo; nó không lưu từng dữ liệu gửi. Trace bản sửa ghi `SEND,1`, `SEND,2`, rồi `RECEIVE,2`: số 2 ở dòng nhận là **số sự kiện**, không phải dữ liệu gốc của lần gửi thứ hai. Vì vậy bản sửa không mất số lần gửi trong phép thử này, nhưng không thể khôi phục riêng payload 1 và 2 từ notification value. Nếu cần đủ từng payload thì phải có buffer hoặc queue lưu dữ liệu.
