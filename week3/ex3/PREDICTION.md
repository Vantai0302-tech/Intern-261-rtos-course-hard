# W3.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

Với MỖI loại fault trong `W3E3_ASSIGNED_FAULT_NAMES` (PARAMS.md), dự đoán TRƯỚC khi tự kích hoạt:

| Loại fault | CFSR bit dự đoán sẽ set | BFAR/MMFAR có hợp lệ không | Dòng code dự đoán gây fault |
|---|---|---|---|
| INVALID_PC_JUMP |	INVSTATE (bit 17) | BFAR và MMFAR không hợp lệ | Lệnh nhảy qua con trỏ hàm có bit Thumb bằng 0 |
| STACK_OVERFLOW_WRITE | PRECISERR (bit 9), BFARVALID (bit 15) nếu ghi đến địa chỉ gây precise BusFault| Dự đoán BFAR hợp lệ và chứa địa chỉ ghi lỗi; MMFAR không hợp lệ. Nếu lỗi xảy ra trong quá trình stacking, kết quả có thể khác | Lệnh ghi vượt vùng stack tới địa chỉ không được ánh xạ hoặc không thể ghi |
| UNALIGNED_WORD_ACCESS |UNALIGNED (bit 24), với UNALIGN_TRP được bật | BFAR và MMFAR không hợp lệ | Lệnh truy cập word tại địa chỉ không chia hết cho 4 |
| NULL_DEREF_WRITE |PRECISERR (bit 9), BFARVALID (bit 15) nếu phần cứng báo precise BusFault| Dự đoán BFAR hợp lệ, chứa 0x00000000; MMFAR không hợp lệ nếu không có MemManage fault | Lệnh ghi dữ liệu qua con trỏ NULL tới địa chỉ 0x00000000 |

- Dự đoán canary `W3E3_NOINIT_CANARY` có đọc lại đúng sau MỌI lần reset do fault không, vì sao: Canary 0x742C51A4 đọc lại đúng sau mỗi lần reset có kiểm soát bằng NVIC_SystemReset(), nếu bản ghi nằm trong section .noinit và startup không xóa hoặc ghi đè vùng này. Dự đoán này áp dụng khi nguồn cấp vẫn được giữ trong lúc reset.
