# W2.4 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán barrier
- Dự đoán thời gian (us/cycle) từ khi task CHẬM NHẤT gọi `xEventGroupSync` tới khi CẢ `W2E4_NUM_TASKS` task được
  giải phóng cùng lúc: dưới 1000 µs (tương đương dưới 72.000 CPU cycles ở 72 MHz), với điều kiện cả 6 task đều đã đến barrier trong thời hạn 375 ms và không có task khác chiếm CPU quá lâu..

## Dự đoán bug mất sự kiện
- Dự đoán: với tốc độ gửi/nhận trong code của bạn, có xảy ra mất sự kiện khi dùng `eSetValueWithOverwrite` không?
  Vì sao (so tốc độ gửi vs tốc độ task nhận xử lý): Có xảy ra mất sự kiện khi dùng eSetValueWithOverwrite, nếu sender gửi nhiều notification trước khi receiver kịp xử lý notification trước đó 6Nguyên nhân là eSetValueWithOverwrite luôn ghi đè giá trị notification cũ bằng giá trị mới. Nếu sender gửi nhanh hơn receiver, ví dụ gửi 2 lần trong khi receiver chỉ kịp gọi xTaskNotifyWait() 1 lần, receiver chỉ nhận được giá trị cuối cùng. Vì vậy, số sự kiện nhận được sẽ nhỏ hơn số lần gửi.
- Dự đoán số sự kiện bị mất trên 1000 lần gửi (nếu có): khoảng 500 sự kiện, với giả định sender gửi 2 notification liên tiếp trước mỗi lần receiver được chạy. Khi đó, cứ 2 lần gửi chỉ có 1 lần nhận, nên dự đoán receiver xử lý được khoảng 500 sự kiện và mất khoảng 500 sự kiện.

## Dự đoán sau khi sửa
- Dự đoán sau khi đổi cơ chế: số gửi có luôn bằng số nhận không trong mọi tốc độ gửi? Vì sao:  Sau khi thay eSetValueWithOverwrite bằng eIncrement, số sự kiện gửi sẽ bằng số sự kiện được xử lý trong cùng kịch bản thử nghiệm, với điều kiện bộ đếm không bị tràn và receiver có đủ thời gian xử lý hết các sự kiện đang chờ.Nguyên nhân là eIncrement cộng dồn số lần gửi vào  notification value thay vì ghi đè giá trị cũ. Tuy nhiên, không thể bảo đảm số gửi luôn bằng số nhận ở mọi tốc độ gửi vì bộ đếm có giới hạn 32-bit và notification không lưu riêng dữ liệu của từng sự kiện.

## Dự đoán RAM/cycle
- Dự đoán RAM tĩnh event group so với bản binary semaphore cũ: tăng/giảm bao nhiêu byte, vì sao:giảm 48 byte khi so riêng đối tượng đồng bộ (`StaticEventGroup_t` khoảng 32 byte, `StaticSemaphore_t` khoảng 80 byte trong cấu hình hiện tại). Chưa tính TCB và stack của task..
- Dự đoán cycle trung bình đường notification so với binary semaphore: thấp hơn khoảng 20% cho cùng một cặp gửi/nhận, vì notification dùng trạng thái có sẵn trong TCB thay vì thao tác trên đối tượng semaphore. Đây là dự đoán cần kiểm tra bằng DWT.
