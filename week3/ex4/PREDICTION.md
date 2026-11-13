# W3.4 – PREDICTION (điền TRƯỚC khi chạy GDB, commit trước)

Trước khi kết nối GDB lần đầu, chỉ dựa vào `nm w3e4.elf`/`readelf -s w3e4.elf` (symbol table, KHÔNG chạy board):

- Danh sách symbol liên quan kernel bạn tìm thấy (tên + địa chỉ)
  - `pxCurrentTCB`: `0x20000A60`.
  - `pxReadyTasksLists`: `0x20000A64`.
  - `xDelayedTaskList1`: `0x20000B04`.
  - `xDelayedTaskList2`: `0x20000B18`.
  - `pxDelayedTaskList`: `0x20000B2C`.
  - `pxOverflowDelayedTaskList`: `0x20000B30`.
  - `xPendingReadyList`: `0x20000B34`.
  - `xSuspendedTaskList`: `0x20000B60`.
  - `uxCurrentNumberOfTasks`: `0x20000B74`.
  - `xTickCount`: `0x20000B78`.

- Symbol task và mutex liên quan:
  - `s_task_a_tcb`: `0x2000091C`.
  - `s_task_b_tcb`: `0x200006C0`.
  - `s_mutex1`: `0x2000097C`.
  - `s_mutex2`: `0x20000978`.

- Dự đoán ban đầu (trước khi xem trạng thái RAM) về khả năng cao nhất trong 3 loại lỗi (deadlock / stack
  overflow / sai priority), chỉ dựa trên số lượng task/mutex/ISR thấy được qua symbol: deadlock, vì sao: vì symbol table có hai task ứng dụng
  `vTaskA`, `vTaskB` và hai mutex `s_mutex1`, `s_mutex2`. Hai task có
  thể giữ và chờ mutex của nhau nếu lấy khóa theo thứ tự ngược nhau.
- Dự đoán số lệnh GDB bạn sẽ cần để xác nhận chắc chắn (so với ngưỡng tối thiểu
  `W3E4_MIN_GDB_EVIDENCE_COMMANDS` trong PARAMS.md): Dự đoán cần khoảng 10 lệnh GDB khác nhau để kiểm tra task hiện tại,
  danh sách Ready/Blocked, chủ sở hữu mutex, stack và priority ISR.
  Số này lớn hơn ngưỡng tối thiểu `W3E4_MIN_GDB_EVIDENCE_COMMANDS = 6`.
