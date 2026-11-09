# W3.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán trôi tick
- Nguồn clock RTC bạn chọn (LSE/LSI) và dung sai danh định (ppm) theo datasheet: Chọn LSE 32.768 kHz vì sử dụng thạch anh ngoài. Tạm giả định dung sai thạch anh là ±20 ppm, giá trị này cần được xác nhận theo datasheet linh kiện ở trên board.
- Dự đoán trôi tick đo được trong `W3E2_DRIFT_TEST_WINDOW_MIN` phút (ms), so với ngưỡng `W3E2_DRIFT_BOUND_PPM`
  trong PARAMS.md: Với giả định thời gian thử nghiệm là 10 phút, và sai số RTC là 20 ppm thì độ trôi do riêng sai số tần số oscillator dự kiến khoảng ±12 ms. Sai số thực tế còn phụ thuộc vào độ phân giải RTC, cách làm tròn khi bù tick bằng vTaskStepTick() và thời gian chuyển đổi giữa Active/STOP.

## Dự đoán dòng tiêu thụ
- Dự đoán dòng tiêu thụ active (mA): 25 mA.
- Dự đoán dòng tiêu thụ idle/STOP mode (mA hoặc µA): 5 mA.
- Tỉ lệ giảm dòng dự đoán giữa active và idle: 25/5 = 5 lần.
