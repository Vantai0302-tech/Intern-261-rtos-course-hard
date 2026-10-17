# W1.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán thời gian context switch
- Dự đoán min/max cycle context switch ở `-O0`: 180 / 400 cycle.
- Dự đoán min/max cycle context switch ở `-O2`: 100 / 250 cycle.
- Lý do dự đoán `-O2` nhanh hơn/chậm hơn (dựa trên những gì bị tối ưu): nhanh hơn vì Compiler tối ưu các phép tính, bỏ các thao tác thừa, giảm số lệnh trong khi thực thi của scheduler và tracer

## Dự đoán đối chiếu Gantt
- Sai số kỳ vọng giữa tracer và logic analyzer (so với `W1E2_CORRELATION_TOLERANCE_US` trong PARAMS.md): 5 us.