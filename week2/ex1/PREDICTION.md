# W2.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán trình tự ngắt bình thường
- Số lần ngắt EV (event) cho 1 transaction write len=N byte:  khoảng N + 2 đến N + 3 lần tùy cách xử lý TXE/BTF và trạng thái ITBUFEN. 
- Thời điểm (theo DWT cycle hoặc us) dự kiến giữa START và EV5: 10 µs (720 cycles) .

## Dự đoán bus recovery
- Số xung SCL cần toggle để slave nhả SDA trong trường hợp xấu nhất: 9 (tối đa lý thuyết: 9).
- Dự đoán: hệ thống có phục hồi được đủ `W2E1_RECOVERY_REPEAT_COUNT` lần liên tiếp không? Vì sao: Dự đoán có. Vì nếu SDA không bị giữ LOW vĩnh viễn và driver thực hiện đúng trình tự: vô hiệu hóa I2C peripheral → chuyển SCL/SDA sang GPIO open-drain → tạo tối đa 9 xung SCL → phát STOP → cấu hình lại I2C1. Mỗi lần recovery phải đưa bus về trạng thái rảnh trước khi bắt đầu transaction tiếp theo
- Dự đoán thời gian (ms) từ lúc phát hiện bus-stuck tới khi transaction tiếp theo thành công: Khoảng 1–5 ms, giả sử slave nhả SDA sau các xung SCL, quá trình khởi tạo lại I2C diễn ra bình thường và transaction tiếp theo không gặp lỗi.

## Dự đoán timeout
- Với `W2E1_I2C_TIMEOUT_MS` cho trong `PARAMS.md`, tình huống nào sẽ KHÔNG kịp timeout trước khi watchdog (nếu có) can thiệp? Nếu watchdog còn ít hơn `W2E1_I2C_TIMEOUT_MS` trước khi hết hạn mà không được refresh, MCU có thể bị reset trước khi task phát hiện I2C timeout. Ngoài ra, nếu FreeRTOS tick bị trì hoãn do interrupt bị vô hiệu hóa quá lâu hoặc task xử lý bị starvation, timeout có thể không được xử lý kịp trước khi watchdog can thiệp.
