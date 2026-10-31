# W1.4 – REPORT

## Log phân mảnh heap_4
Log `w1e4_log.txt` đã thu đủ 48 bước, gồm 3 vòng; mỗi vòng cấp phát 8 khối rồi giải phóng đủ 8 khối theo thứ tự params.h.

| Vòng | Heap trống thấp nhất (bytes) | Heap trống sau khi giải phóng đủ (bytes) |
|---|---:|---:|
| 1 | 6896 | 8176 |
| 2 | 6896 | 8176 |
| 3 | 6896 | 8176 |

## Giải thích phân mảnh (ĐỊNH LƯỢNG)
Tổng kích thước yêu cầu là 1216 byte. Mỗi khối thêm 8 byte quản lý, tổng 1280 byte cho 8 khối. Heap trống giảm từ 8176 xuống 6896 byte, đúng 1280 byte, rồi trở lại 8176 ở bước 16, 32 và 48. Không có phần heap trống bị giảm dần sau các vòng.

Thứ tự free có thể tạo những khoảng trống tách rời khi các khối ở giữa còn được giữ. heap_4 gộp các khối tự do liền kề theo địa chỉ. Hai hàm đo trong log chỉ cho tổng heap trống và mức thấp nhất từng đạt, không đo trực tiếp số mảnh hay kích thước mảnh lớn nhất. Không dùng hai số này để suy ra mức phân mảnh ở từng bước.

Dự đoán ban đầu ghi 1216 byte bị mất do phân mảnh là sai: đó là tổng payload. `free_bytes_min_ever` giữ ở 6896 vì đây là mức thấp nhất trong lịch sử, không phải số byte còn thiếu sau khi free.

## Bảng RAM tĩnh vs .map

Các bản chuyển đổi: `w1e1_static` cho blinky, `w1e2_static` cho tracer và `w1e4_static` cho bốn task RTA. Bản cuối dùng queue tĩnh để chuyển số đo stack từ task lấy mẫu sang task in UART, và mutex tĩnh bảo vệ UART. `configSUPPORT_DYNAMIC_ALLOCATION=0` ở các target này. Không liên kết heap_4.c; kiểm tra symbol của cả ba ELF không thấy `pvPortMalloc`, `vPortFree` hay `ucHeap`.

Bảng dưới phân rã RAM sau build cho `build/debug/w1e4_static.map`; đây không phải số dự đoán trước khi chạy.

| Section/đối tượng | Phân rã sau build (bytes) | Từ .map (bytes) | Lệch |
|---|---|---|---|
| .data | 24 | 24 | 0 |
| .bss, đã gồm TCB/stack/buffer tĩnh | 8824 | 8824 | 0 |
| .noinit | 0 | 0 | 0 |
| Stack main/ISR (.stack) | 1024 | 1024 | 0 |
| **Tổng** | **9872** | **9872** | **0** |

Trong .bss: dữ liệu ứng dụng RTA 3508 byte; phần đo stack/queue/mutex và bộ nhớ Idle 2924 byte; tracer 2061 byte; biến kernel/port 324 byte; padding 7 byte. Tổng là 8824 byte. Các stack này đã nằm trong .bss nên không cộng lại vào tổng RAM.

Vùng còn trống giữa cuối .noinit và stack main/ISR là 10608 byte. Dòng linker báo RAM 100% do .stack được đặt ở đỉnh RAM; không có nghĩa mọi byte ở giữa đều chứa dữ liệu. Software timer hiện tắt nên không có Timer task stack trong ELF này; callback cấp bộ nhớ Timer đã có cho trường hợp bật timer.

PREDICTION.md ban đầu dự đoán tổng 20480 byte, lệch 10608 byte so với bản tĩnh này, vượt dung sai 255 byte. Không sửa dự đoán sau khi đo; cần ghi nhận dự đoán ban đầu chưa phản ánh kích thước các đối tượng thực tế. Bảng phân rã sau build khớp .map nhưng không biến dự đoán ban đầu thành đạt.

### Đo stack trên board

Chưa có số đo high-water mark trên board cho các bản tĩnh. Chạy từng bản ít nhất 15 giây; với bản tracer/RTA, gửi `D` để đi qua cả đường dump UART. Log `# stack` in stack đã cấp và mức trống nhỏ nhất theo word và byte. Các task đo thêm có ảnh hưởng lịch chạy; dùng bản w1e3 riêng cho báo cáo RTA trước đó.

### Kiểm tra API

Kiểm tra các nguồn dùng cho bản tĩnh: PASS (23 files). Khi kiểm tra toàn thư mục week1/ex4, script vẫn báo FAIL tại pvPortMalloc trong heap_experiment.c của bước thí nghiệm heap. File này chỉ được liên kết vào w1e4, không nằm trong các ELF tĩnh; chưa thể ghi toàn thư mục PASS.

## Trả lời câu hỏi trade-off
Trong bản tĩnh, bộ nhớ task, queue và mutex đã được dành sẵn trong .bss, không có quá trình malloc/free tạo các khoảng trống trong heap. Đổi lại, kích thước stack và buffer phải được chọn trước. Bản w1e4_static đã dành 9872 byte RAM theo .map; mức stack có thể giảm an toàn đến đâu còn phải chờ số đo high-water mark trên board, không suy từ kích thước khai báo.
