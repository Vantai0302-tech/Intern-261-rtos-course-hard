# W1.4 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán phân mảnh
- Với thứ tự free `W1E4_FREE_ORDER` của bạn (xem PARAMS.md), dự đoán: có phân mảnh hay không? Vì sao
  (dựa trên coalescing theo địa chỉ liền kề của heap_4)? Có sự phân mảnh tạm thời vì một số block được giải phóng  lại không nằm liền kề theo địa chỉ. Sau khi giải phóng đủ cả 8 khối, heap_4 gộp các khối kề nhau.
- Dự đoán `xPortGetMinimumEverFreeHeapSize()` cuối cùng so với heap ban đầu: 1216 bytes bị "mất" do phân mảnh (nếu có).

## Dự đoán ngân sách RAM tĩnh
- Tổng RAM dự kiến (.data + .bss + stacks tĩnh + buffer tĩnh), tính tay: 20480 bytes.
- So với 20 KB RAM vật lý, còn dư: 255 bytes.
