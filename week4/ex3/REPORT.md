# W4.3 – REPORT

## Bảng dự đoán RTA vs đo (chạy ổn định)

| Task | WCRT dự đoán (W4.1, us) | WCRT đo được (us) | Sai số (%) | Trong ngưỡng? |
|---|---|---|---|---|
| control | 118 | 163 | 38,14 | Không |
| imu | 1528 | 1315 | 13,94 | Không |
| telemetry | 1778 | 2388 | 34,31 | Không |
| shell | 1928 | 1248 | 35,27 | Không |
| idle_aux | 5868 | 965 | 83,55 | Không |

CPU load đo được: 13,26 % (trung bình; dao động 13,12–13,32 %). Stack high-water-mark thấp nhất còn lại (tất cả task): 24 words.

Đo 600 giây, lấy mẫu mỗi giây; log: [w4e3_normal_final.txt](w4e3_normal_final.txt). Cả năm task có deadline miss, invalid job và mất log bằng 0. Có 15600 job telemetry trong cửa sổ đo; host thu 15628 khung trong toàn phiên, gồm trước/sau cửa sổ đo, lỗi CRC bằng 0. Không phát sinh thêm lỗi I2C, UART hay tràn RX trong cửa sổ normal.

RTA lấy từ [DESIGN.md W4.1](../ex1/DESIGN.md); sai số = |đo − dự đoán| / dự đoán × 100 %, ngưỡng 13 %. Stack được đọc định kỳ bằng `uxTaskGetStackHighWaterMark`, phản ánh mức thấp nhất từ lúc khởi động. Năm task ứng dụng còn ít nhất: control 195, imu 301, telemetry 215, shell 187, idle_aux 117 words.

WCRT lấy mốc phát hành đến hoàn tất job bằng TIM4 ở 1 MHz. CPU tính từ chênh lệch runtime tổng và Idle qua `ulTaskGetIdleRunTimeCounter()`.

## Log từng kịch bản fault

| Fault | Lệnh gửi tại thời điểm host | Quan sát control | Suy giảm đã công bố W4.1? | Watchdog / lý do reset |
|---|---|---|---|---|
| TASK_HANG | +0,524 s | Không ghi nhận trễ trước reset; reset làm gián đoạn chu kỳ | Không | IWDG / TASK_HANG; hệ thống sẵn sàng lại sau 206,971 ms |
| UART_FLOOD | +0,413 s | WCRT 224 us, không trễ deadline; MCU bỏ 272405 byte RX | Có | Không reset |
| SENSOR_UNPLUG | +0,629 s | WCRT 146 us, không trễ deadline; mẫu INVALID, đầu ra bằng 0 | Có | Không reset |
| CPU_OVERLOAD | +3,069 s | Bị bỏ đói ít nhất 82927 us, không hoàn tất job trước reset | Không | IWDG / CPU_OVERLOAD; hệ thống sẵn sàng lại sau 207,978 ms |

Log: [watchdog](w4e3_watchdog_final.txt), [UART flood](w4e3_uart_flood_final.txt), [rút cảm biến](w4e3_sensor_final.txt). Mốc host là thời điểm gửi lệnh, không phải thời điểm chính xác CPU vào hàm gây lỗi.

Thời gian watchdog đo từ lúc firmware kích hoạt lỗi đến khi IMU có mẫu hợp lệ và control/IMU tiếp tục tiến triển. RTC dùng LSE 32768 Hz và được giữ chạy qua reset ([RM0008](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)); độ phân giải danh định 30,52 us, dự phòng lượng tử 2 tick khoảng 61,04 us. TASK_HANG và CPU_OVERLOAD lần lượt thấp hơn giới hạn 302 ms là 95,029 ms và 94,022 ms.

WCRT quan sát của các job hoàn tất trong từng cửa sổ fault:

| Task | RTA W4.1 (us) | TASK_HANG (us) | UART_FLOOD (us) | SENSOR_UNPLUG (us) | CPU_OVERLOAD (us) |
|---|---:|---:|---:|---:|---:|
| control | 118 | 102 | 224 | 146 | NA |
| imu | 1528 | 1087 | 1272 | 1590 | NA |
| telemetry | 1778 | 998 | 1457 | 2433 | NA |
| shell | 1928 | 630 | 800931 | 903 | NA |
| idle_aux | 5868 | NA | 3649 | 687 | NA |

`NA` là không có job hoàn tất trước reset, không phải WCRT bằng 0. idle_aux bị treo trong TASK_HANG; cả năm task bị task priority 9 bỏ đói trong CPU_OVERLOAD. Bộ đếm deadline miss của job hoàn tất bằng 0 trong trường hợp này không chứng minh các job đang chờ đúng hạn.

UART flood gửi 276480 byte trong khoảng 3,052 s. Shell có 65/67 job hoàn tất trễ deadline; WCRT 800931 us, trễ tối đa 750931 us so với deadline 50000 us. Control không ghi nhận deadline miss.

Khi rút SDA, khung telemetry seq=3267 ghi `imu_valid=0`, đầu ra control bằng 0. Sau khi cắm lại, timestamp mẫu hợp lệ tiếp tục tăng; lỗi I2C tăng 6, recovery tăng 6, control overrun không tăng. Cửa sổ đo gồm chờ thao tác, đoạn rút SDA và đoạn ổn định sau khi cắm lại.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- CPU trung bình 13,26 %, cao hơn dự đoán 12 % là 1,26 điểm phần trăm. Lần đo có lấy mẫu, xuất log và gửi lệnh `status`; chưa đo riêng chi phí từng phần.
- Normal: control cao hơn RTA 45 us, telemetry cao hơn 610 us. Telemetry có cả thời gian chờ DMA; chưa tách được nguyên nhân của toàn bộ chênh lệch.
- IMU thấp hơn RTA 213 us, shell thấp hơn 680 us, idle_aux thấp hơn 4903 us. RTA dùng ngân sách recovery, còn normal không có recovery mới. W4.3 giám sát AUX mỗi 20 ms, recovery vẫn giới hạn mỗi 100 ms; shell chủ yếu xử lý `status`.
- Phiên UART flood ghi nhận jitter lũy kế tăng từ 74 lên 172 us, vượt ngưỡng 109 us là 63 us. Normal bắt đầu và kết thúc đều giữ 172 us; không dùng giá trị lũy kế này làm số jitter riêng của normal.
- RTA W4.1 chưa có task priority 9 gây quá tải, nên không áp dụng được để chặn trên response time của CPU_OVERLOAD.
## Trả lời câu hỏi trade-off

CPU_OVERLOAD làm suy giảm điều khiển nhiều nhất: task priority 9 cao hơn control priority 8, khiến control không được chạy trong ít nhất 82927 us, hơn 82 chu kỳ 1 ms. Watchdog đưa hệ thống trở lại trạng thái có mẫu hợp lệ sau 207,978 ms, nhưng không giữ được deadline trong thời gian quá tải. Cần giới hạn tải hoặc bổ sung cơ chế suy giảm để bảo vệ deadline của control.
