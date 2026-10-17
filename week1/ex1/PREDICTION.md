# W1.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán tần số
- Flash latency dự kiến ở 72 MHz (theo bảng wait-state RM0008 §4): 2 wait-state.
- Tần số xung TIM đo được dự đoán: 1000 Hz ± `W1E1_CLOCK_TOLERANCE_PPM` (xem PARAMS.md) = 999.718 Hz đến 1000.282 Hz.

## Dự đoán hành vi blinky
- Chu kỳ blink task A / B dự kiến (xem PARAMS.md): 379 ms / 711 ms.
- Ảnh hưởng của priority 2 task tới thứ tự chạy khi cùng ready: Vì 2 task đều được set up priority = 1 nên không có task được ưu tiên. Khi đó, FreeRTOS lập lịch giữa 2 task luân phiên.

## Rủi ro đã lường trước
- Nếu quên set flash latency trước khi tăng PLL: dự đoán hệ quả chương trình có thể sẽ bị treo.
- Nếu PendSV priority không thấp nhất: dự đoán hệ quả PendSV có thể ngắt các ISR có độ ưu tiên thấp hơn nó, gây sai về thứ tự xử lí ngắt