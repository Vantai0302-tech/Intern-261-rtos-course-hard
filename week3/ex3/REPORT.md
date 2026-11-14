# W3.3 – REPORT

## Số đo

Với MỖI loại fault trong `W3E3_ASSIGNED_FAULT_NAMES`:

| Loại fault | CFSR (hex) | HFSR (hex) | BFAR/MMFAR (hex, valid?) | PC stacked | Dòng code (`addr2line`) |
|---|---|---|---|---|---|
| INVALID_PC_JUMP | `0x00020000` | `0x40000000` | BFAR và MMFAR không hợp lệ | `0x0800174A` | `trigger_invalid_pc_fault()`, `main.c:45` |
| STACK_OVERFLOW_WRITE | `0x00000082` | `0x40000000` | BFAR không hợp lệ; MMFAR = `0x2000019C`, hợp lệ | `0x08001832` | `trigger_stack_fault()`, `main.c:117` |
| UNALIGNED_WORD_ACCESS | `0x01000000` | `0x40000000` | BFAR và MMFAR không hợp lệ | `0x0800170C` | `trigger_unaligned_fault()`, `main.c:24` trong `w3e3_unaligned.elf` |
| NULL_DEREF_WRITE | `0x00000082` | `0x40000000` | BFAR không hợp lệ; MMFAR = `0x00000000`, hợp lệ | `0x080017FC` | `trigger_null_fault()`, `main.c:97` |

- Canary đọc lại sau mỗi lần reset: khớp `0x742C51A4` trong cả 4 lần đo.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- `INVALID_PC_JUMP` có `INVSTATE` ở bit 17 và `UNALIGNED_WORD_ACCESS` có `UNALIGNED` ở bit 24, khớp dự đoán. NULL và stack ban đầu dự đoán `PRECISERR/BFARVALID`, nhưng thực tế dùng MPU bảo vệ địa chỉ NULL và guard dưới stack nên nhận `DACCVIOL` ở bit 1, `MMARVALID` ở bit 7, tổng CFSR = `0x82`. Vì vậy địa chỉ lỗi nằm trong MMFAR.
- Không có lần nào canary không khớp trong bốn lần đo.

## Trả lời câu hỏi trade-off

Dùng stacked PC để tra dòng code bằng `addr2line` trên đúng ELF, rồi đối chiếu disassembly và các thanh ghi toán hạng nếu có. Với lỗi unaligned, BFAR/MMFAR đều không hợp lệ nhưng PC = `0x0800170C` trỏ tới `ldr r0, [r3]`. Disassembly của ELF unaligned cho thấy địa chỉ đọc là `0x2000080D`, lệch 1 byte so với căn chỉnh word. Nhờ đó xác định được lệnh và địa chỉ gây lỗi. Với địa chỉ tính động, cần lưu thêm thanh ghi toán hạng hoặc kiểm tra bằng GDB.
