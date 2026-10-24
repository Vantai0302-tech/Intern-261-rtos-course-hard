# W2.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán đúng đắn
- Với `W2E2_STRESS_COUNT` phần tử (xem PARAMS.md), dự đoán: ring buffer tự viết có mất/lặp phần tử nào không? Vì
  sao (dựa trên memory barrier bạn đặt): Với `W2E2_STRESS_COUNT = 1000000`, ring buffer tự viết không mất hoặc lặp phần tử nếu  ISR chỉ tăng sequence counter sau khi push thành công; khi buffer đầy thì thử lại, không bỏ qua phần tử. Producer ghi phần tử rồi dùng `__DMB()` trước khi cập nhật `head`; consumer quan sát `head` rồi mới đọc phần tử, và chỉ cập nhật `tail` sau khi đọc xong. Mỗi chỉ số chỉ có một phía ghi..

## Dự đoán hiệu năng
- Dự đoán cycle trung bình 1 lần push/pop trên ring buffer tự viết: 35/30 cycle.
- Dự đoán cycle trung bình 1 lần `xQueueSendFromISR` + `xQueueReceive`: 250 cycle.
- Dự đoán ring buffer nhanh hơn/chậm hơn queue gốc bao nhiêu lần, và vì sao (copy, critical section, wake task):  Dự đoán ring buffer nhanh hơn khoảng 250 / 35 ≈ 7,1 lần. Queue phải thực hiện thêm việc sao chép phần tử, bảo vệ cấu trúc nội bộ và kiểm tra task cần đánh thức. Mức chênh lệch thực tế phụ thuộc việc benchmark có gây chuyển task hay không.
