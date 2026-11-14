# W3.4 – REPORT

## Số đo (bằng chứng GDB)


| # | Lệnh GDB | Output (nguyên văn, rút gọn hợp lý) | Kết luận rút ra từ lệnh này |
|---|---|---|---|
| 1 | `p *(TCB_t *)&s_task_a_tcb` | `pvContainer = 0x200009a4 <s_mutex2_storage+36>`; `uxMutexesHeld = 1` | task_a giữ một mutex, đang chờ mutex2. |
| 2 | `p *(TCB_t *)&s_task_b_tcb` | `pvContainer = 0x200009f4 <s_mutex1_storage+36>`; `uxMutexesHeld = 1` | task_b giữ một mutex, đang chờ mutex1. |
| 3 | `p *(Queue_t *)s_mutex1` | `xMutexHolder = 0x2000091c <s_task_a_tcb>`; `pxNext = 0x200006d8 <s_task_b_tcb+24>` | Mutex1 do task_a giữ; task_b nằm trong danh sách chờ nhận. |
| 4 | `p *(Queue_t *)s_mutex2` | `xMutexHolder = 0x200006c0 <s_task_b_tcb>`; `pxNext = 0x20000934 <s_task_a_tcb+24>` | Mutex2 do task_b giữ; task_a nằm trong danh sách chờ nhận. |
| 5 | `p pxReadyTasksLists` | `uxNumberOfItems = 1, pxIndex = 0x20000468 <s_idle_tcb+4>` | Priority 0 có Idle; các priority 1–7 đều có số phần tử bằng 0 trong log. |
| 6 | `p xSuspendedTaskList` | `uxNumberOfItems = 3` | Có ba task trong danh sách, gồm hai task ứng dụng đang chờ mutex và timer task. |
| 7 | `p sizeof(s_task_a_stack)` | `$12 = 512` | Stack task_a được cấp 512 byte. |
| 8 | `p sizeof(s_task_b_stack)` | `$13 = 512` | Stack task_b được cấp 512 byte. |
| 9 | `x/32xw &s_task_a_stack` | `0x2000071c <s_task_a_stack>: 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5` | Cả 32 word được đọc vẫn là mẫu A5: 128 byte đầu stack chưa bị ghi đè. |
| 10 | `x/32xw &s_task_b_stack` | `0x200004c0 <s_task_b_stack>: 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5` | Cả 32 word được đọc vẫn là mẫu A5: 128 byte đầu stack chưa bị ghi đè. |
| 11 | `p/x *(unsigned int *)0xE000ED28` | `$14 = 0x0` | CFSR không ghi nhận fault. |
| 12 | `p/x *(unsigned int *)0xE000ED2C` | `$15 = 0x0` | HFSR không ghi nhận HardFault. |
| 13 | `x/3xw 0xE000E100` | `0xe000e100: 0x00000000 0x00000000 0x00000000` | Không có IRQ ngoài đang được bật. |
| 14 | `x/wx 0xE000ED20` | `0xe000ed20: 0xf0f00000` | PendSV và SysTick cùng có priority 0xF0, mức thấp nhất với 4 bit priority. |

## Giải thích sai lệch (ĐỊNH LƯỢNG so với PREDICTION.md)

- Dự đoán ban đầu là deadlock, khớp kết quả đọc RAM. Symbol chỉ cho biết có hai task và hai mutex; RAM mới xác nhận được vòng chờ task_a → mutex2 → task_b → mutex1 → task_a.
- Phiên log có 22 lệnh đọc symbol/RAM khác nhau, nhiều hơn dự đoán 10 lệnh là 12 lệnh và vượt mức tối thiểu 6 lệnh. Bảng trên trình bày 14 lệnh chính.

## Nguyên nhân gốc

- Kết luận: deadlock do hai task lấy mutex theo thứ tự ngược nhau.
- Bằng chứng XÁC NHẬN: #1–#4 cho thấy task_a giữ mutex1 và chờ mutex2, còn task_b giữ mutex2 và chờ mutex1. Cả hai đang chờ vô hạn trong danh sách ở #6.
- Bằng chứng LOẠI TRỪ 2 khả năng còn lại: stack overflow không phù hợp với #7–#10 vì 128 byte đầu mỗi stack vẫn nguyên mẫu A5; con trỏ stack trong #1–#2 vẫn nằm trong vùng cấp phát 512 byte. Sai priority ISR không giải thích trạng thái này: #13 không có IRQ ngoài được bật, #14 cho thấy PendSV/SysTick ở mức thấp nhất, trong khi #1–#4 xác nhận trực tiếp vòng chờ mutex.
