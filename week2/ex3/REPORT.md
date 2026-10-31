# W2.3 – REPORT

## Số đo

- Inversion dùng binary semaphore: [Gantt](gantt_inversion.svg), [log UART](w2e3_trace_inversion.txt).
- Chặn nối tiếp dùng mutex có inheritance: [Gantt](gantt_chained.svg), [log UART](w2e3_trace_chained.txt).
- Deadlock: [Gantt](gantt_deadlock.svg), [log UART](w2e3_trace_deadlock.txt).
- Sau khi dùng Priority Ceiling: [Gantt inversion](gantt_ceiling_inversion.svg), [log inversion](w2e3_ceiling_inversion.txt), [Gantt hai khóa](gantt_ceiling.svg) và [log hai khóa](w2e3_ceiling.txt).

Các giá trị bên dưới được đổi từ DWT cycles sang mili giây theo tần số CPU 72 MHz.

| Cách khóa | Số lần H bị chặn đáng kể | Tổng thời gian H chờ |
|---|---:|---:|
| Binary semaphore, inversion | 1 | 17.076.074 cycles = 237,168 ms |
| Mutex có inheritance, chặn nối tiếp | 2 | 5.491.332 cycles = 76,269 ms |
| Priority Ceiling, phép thử inversion | 1 | 2.535.826 cycles = 35,220 ms |
| Priority Ceiling, phép thử hai khóa | 1 | 2.891.118 cycles = 40,154 ms |

Với mutex có inheritance, H chờ A 2.672.870 cycles (37,123 ms), sau đó chờ B 2.818.462 cycles (39,145 ms). Sau khi dùng ceiling, H chờ A 2.889.174 cycles (40,127 ms) và chờ B 1.944 cycles (0,027 ms). Log cuối ghi `L=1 M1=1 H=1`: cả ba task đều hoàn tất. Số 237,168 ms ở hàng binary semaphore được đo khi M2 chạy hữu hạn 200 ms; đó không phải cận trên cho priority inversion.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Trong PREDICTION.md, dự đoán H bị chặn 37 + 39 = 76 ms khi dùng mutex có inheritance. Kết quả là 76,269 ms, cao hơn 0,269 ms. Jitter chuyển task ở bản `-O0` của W1.2 là 171 cycles, tương đương 2,375 µs. Con số này nhỏ hơn nhiều so với 0,269 ms, nên riêng jitter chuyển task không giải thích hết sai lệch; phép đo còn tính thời gian đánh thức task, vào/ra hàm khóa và các lần chuyển task.

Dự đoán H chờ khoảng 39 ms khi dùng ceiling. Phép thử hai khóa đo được 40,154 ms, cao hơn 1,154 ms. H bắt đầu chờ khi M1 đang giữ B; thời gian đo còn gồm phần trước khi M1 bắt đầu chạy đoạn giữ khóa B 39 ms. Tôi chưa đo riêng từng phần overhead, nên không quy toàn bộ 1,154 ms cho wrapper.

Trên Gantt của phép thử inheritance, H bị chuyển ra sau khi xin A, chạy lại khi L nhả A, rồi bị chuyển ra lần nữa khi xin B và chỉ chạy lại sau khi M1 nhả B. Đây là hai lần chặn bởi hai task khác nhau. Với binary semaphore, H chỉ chờ một khóa, nhưng M2 chạy chen làm L chậm nhả khóa. Sau khi dùng ceiling cho hai khóa, H chờ A trong lúc M1 giữ B; khi M1 hoàn tất, H chạy tiếp và lấy B sau khoảng 0,027 ms.

## Trả lời câu hỏi trade-off

Ở phép thử deadlock, L giữ A rồi chờ B, còn M1 giữ B rồi chờ A. Log tại 100 ms và 200 ms đều ghi `L=2 M1=2`; giá trị 2 nghĩa là task đang chờ khóa thứ hai. Hai task không thể tự nhả khóa đang giữ.

Trong wrapper Priority Ceiling, một task chỉ được lấy khóa mới nếu priority gốc của nó cao hơn ceiling của những khóa do task khác giữ. Vì vậy, khi M1 đang giữ B, L chưa thể lấy A để tạo thành vòng chờ A ↔ B. Ở phép thử ceiling, M1 hoàn tất trước, rồi H và L cũng hoàn tất (`L=1 M1=1 H=1`). Gantt sau ceiling không còn đoạn cả L và M1 cùng dừng để chờ khóa của nhau.

Phép thử deadlock gốc dùng notification để ép thứ tự chạy, còn phép thử ceiling dùng thời điểm kích hoạt khác. Kết quả trên chứng minh thứ tự lấy khóa ngược nhau A→B và B→A không tạo deadlock trong lần chạy ceiling; không coi đây là phép đo chỉ thay wrapper mà giữ nguyên toàn bộ lịch kích hoạt.
