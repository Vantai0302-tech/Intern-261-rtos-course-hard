# W2.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán unbounded inversion
- Với `W2E3_TASK_PRIORITY`/`W2E3_LOCK_HOLD_MS_A` trong PARAMS.md, dự đoán thời gian τ_H bị trễ (block gián tiếp
  qua τ_M) trong trường hợp xấu nhất: không có cận trên hữu hạn.

## Dự đoán chained blocking
- Dự đoán số LẦN τ_H bị chặn nối tiếp (chuỗi blocking) khi dùng mutex có inheritance: 2 lần.
- Dự đoán tổng thời gian block của τ_H trong trường hợp này: 37 + 39 = 76 (37 ms giữ A + 39 ms giữ B) ms.

## Dự đoán deadlock
- Dự đoán: 2 task nào sẽ deadlock, tại dòng/hàm nào, với thứ tự lock nào? Dự đoán: τ_L và τ_M1 deadlock. τ_L lấy khóa A rồi chờ khóa B tại xSemaphoreTake(mutex_B, portMAX_DELAY); τ_M1 lấy khóa B rồi chờ khóa A tại xSemaphoreTake(mutex_A, portMAX_DELAY). Khi mỗi task đã giữ khóa đầu tiêntrước lúc xin khóa thứ hai, cả hai sẽ chờ nhau mãi..

## Dự đoán ceiling
- Dự đoán thời gian block của τ_H sau khi dùng Immediate Priority Ceiling: tối đa khoảng 39 ms (so với 2 trường hợp trên).
- Dự đoán: ceiling có loại bỏ hoàn toàn chained blocking không, hay chỉ giảm số lần? ceiling loại bỏ chained blocking theo nghĩa τ_H không còn bị chặn nối tiếp 2 lần; τ_H vẫn có thể bị chặn 1 lần bởi một đoạn giữ khóa.
