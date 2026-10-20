# W1.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Bảng task set (từ PARAMS.md)
| Task | T (ms) | D (ms) | C (us) | Priority (RM) |
|---|---|---|---|---|
| τ0 |40 |40 |7485 |4|
| τ1 |80 |80 |10029|3|
| τ2 |120|120|19288|3|
| τ3 |150|150|20668|1|

## Utilization
- Uᵢ từng task: U₀ = 0.187125, U₁ = 0.125363, U₂ = 0.160733, U₃ = 0.137787
- U tổng: 0.611008
- Giới hạn Liu–Layland (n=4): 4(2^(1/4) − 1) ≈ 0.7568. So sánh: U tổng < Uₗₗ
## Response Time Analysis (bắt buộc nếu U tổng > giới hạn LL, khuyến khích làm luôn cả khi U thấp để đối chiếu)
Với từng task, trình bày các vòng lặp Rⁿ⁺¹ᵢ = Cᵢ + Σⱼ<ᵢ⌈Rⁿᵢ/Tⱼ⌉Cⱼ đến khi hội tụ hoặc vượt Dᵢ.

| Task | R hội tụ (hoặc MISS) | So với D |
|---|---|---|
|τ0 |7.485 | 7.485 <  40 |
|τ1 |17.514| 17.514 < 80 |
|τ2 |36.802| 36.802 < 120|
|τ3 |64.955| 64.955 < 150|


## Kết luận
Task set này **schedulable** vì tất cả các task đều có Rᵢ < Dᵢ

## Dự đoán WCRT đo được
Dự đoán WCRT thực tế mỗi task sẽ lớn hơn RTA lý thuyết bao nhiêu do overhead context switch (lấy số từ W1.2): 

