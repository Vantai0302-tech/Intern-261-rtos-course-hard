# AI_LOG

Tôi dùng AI để hỏi về tài liệu, làm rõ yêu cầu đề, tìm nguyên nhân lỗi
và đối chiếu cách hiểu với kết quả build, UART hoặc logic analyzer.
Các kết luận trong báo cáo dựa trên số đo tôi thu trên board.

## Week 1

### W1.1 – Bring-up

- Prompt: “Tài liệu CMake: cross compiling (toolchain file),
  CMakePresets.json schema cái này tôi lấy ở đâu”
  Nội dung tham khảo: vai trò của toolchain file, preset và tài liệu CMake.

- Prompt: “không ý là cái đó phải viết đầy đủ cho cả 4 tuần để có thể tái sử dụng”
  Nội dung tham khảo: cách tách cấu hình compiler dùng chung khỏi cấu hình
  riêng của từng target.

- Prompt: “vậy cái FreeRTOS-Kernel là tôi phải tự code hay là lấy mã nguồn
  bên ngoài vậy”
  Nội dung tham khảo: phân biệt mã nguồn FreeRTOS được phép lấy từ bên ngoài
  với startup, driver và phần ứng dụng phải triển khai theo đề.

- Prompt: “W1E1_TAG_GPIO_PORT cho tôi hỏi cái này là sao vậy”
  Nội dung tham khảo: ý nghĩa tham số GPIO được sinh theo MSSV.

- Prompt: “Ý là chân PA1 PA2 có cần phải cắm không”
  Nội dung tham khảo: phân biệt chân tạo xung TIM, chân LED/tag và chân dùng
  để quan sát từng task bằng logic analyzer.

- Prompt: “làm sao để cho nó xuống mức 282”
  Nội dung tham khảo: cách tính sai số ppm và những nguyên nhân có thể làm
  tần số đo khác dự đoán. Tôi dùng số đo CSV và ảnh PulseView để kiểm tra.

### W1.2 – Tracer

- Prompt: “bạn coi thử xem tracer.c”
  Nội dung tham khảo: cấu trúc bản ghi trace, timestamp DWT, task tag và
  cách xử lý khi buffer đầy.

- Prompt: “như này đúng không” (kèm phần so sánh ring buffer và buffer
  dừng khi đầy)
  Nội dung tham khảo: phân biệt ghi vòng với việc dừng ghi sau 256 record.

- Prompt: “cái dassembly là sao”
  Nội dung tham khảo: disassembly là mã lệnh máy được dịch từ C; dùng để
  giải thích chênh lệch thời gian giữa bản -O0 và -O2.

- Prompt: “Sai lệch Gantt vs logic analyzer: ___ us...”
  Nội dung tham khảo: cách đối chiếu timestamp tracer với cạnh GPIO và
  tính min/max/jitter bằng cycle. Tôi kiểm tra bằng log UART, Gantt và ảnh LA.

### W1.3 – Phân tích lập lịch

- Prompt: “vậy cho tôi hỏi params.h được dùng làm gì đây”
  Nội dung tham khảo: vai trò của bộ tham số C, T, D được sinh theo MSSV.

- Prompt: “Giờ công việc ex3 của tôi sau khi làm bảng tính xong ở file
  PREDICTION.md là gì”
  Nội dung tham khảo: trình tự từ tính RTA trên giấy đến tạo task và đo WCRT.

- Prompt: “Ở ý 1, vòng while (DWT->CYCCNT - start < cycle) sẽ kết thúc
  sau C micro giây theo đồng hồ... cái này thì sao”
  Nội dung tham khảo: khác nhau giữa thời gian trôi qua và thời gian CPU
  thực sự chạy khi task bị ngắt.

- Prompt: “sao có file tracer nữa vậy”
  Nội dung tham khảo: tracer chỉ ghi lịch chuyển task; muốn tính response
  time còn cần xác định lúc phát hành và hoàn tất từng job.

### W1.4 – Heap và RAM tĩnh

- Prompt: “Hai ô ngân sách RAM tĩnh chưa có số đáng tin để điền...
  dự đoán mà”
  Nội dung tham khảo: cách lập ngân sách RAM trước khi chạy và tránh cộng
  stack/buffer hai lần khi chúng đã nằm trong .bss.

- Prompt: “vậy tôi viết file heap_experiment.c theo đúng yêu cầu đề bài
  thế nào”
  Nội dung tham khảo: trình tự cấp phát/giải phóng và cách đọc
  xPortGetMinimumEverFreeHeapSize().

- Prompt: “sau khi làm file main.c xong tôi làm gì tiếp”
  Nội dung tham khảo: cách kiểm tra build, đọc file .map và đối chiếu với
  bảng RAM dự đoán.

## Week 2

### W2.1 – I2C và bus recovery

- Prompt: “trong tài liệu nó ở trang nào vậy”
  Nội dung tham khảo: tìm sơ đồ master transmitter/receiver trong RM0008
  và đọc các trạng thái EV5, EV6, EV8, EV8_2.

- Prompt: “trong file i2c_driver tôi nên có những phần gì”
  Nội dung tham khảo: chia driver thành khởi tạo, transaction, event ISR,
  error ISR, timeout và bus recovery.

- Prompt: “ISR hiện luôn gửi địa chỉ Write và xử lý EV6 như giao dịch ghi...
  cái này là sao”
  Nội dung tham khảo: khác nhau giữa giao dịch đọc và ghi; thứ tự ACK,
  NACK, STOP khi nhận 1, 2 hoặc nhiều hơn 2 byte.

- Prompt: “bool i2c1_bus_recover(void) hàm này tôi cần viết gì”
  Nội dung tham khảo: chuyển SCL/SDA sang GPIO open-drain, phát tối đa
  9 xung, tạo STOP rồi trả chân về chức năng I2C.

- Prompt: “Trong PulseView không có 5MHz, chỉ có 6 MHz”
  Nội dung tham khảo: chọn tốc độ lấy mẫu đủ cao so với bus I2C 100 kHz
  và dùng decoder I2C để kiểm tra START, địa chỉ, ACK và STOP.

### W2.2 – SPSC ring buffer

- Prompt: “bạn check xem thử cái spsc_ring.c của tôi code đúng yêu cầu
  đề bài chưa”
  Nội dung tham khảo: điều kiện đầy/rỗng, chỉ số head/tail và cách dùng
  đủ 128 phần tử.

- Prompt: “vậy tôi phải làm thế nào” (về head/tail và memory barrier)
  Nội dung tham khảo: dùng bộ đếm tăng liên tục, truy cập mảng bằng mask
  và đặt DMB trước khi công bố dữ liệu/chỗ trống.

- Prompt: “benchmark DWT cái này là viết cái gì vậy”
  Nội dung tham khảo: đo cycle quanh lời gọi push/pop và queue API,
  tách thời gian chạy API khỏi thời gian task chờ.

- Prompt: “Lập luận vì sao SPSC ... an toàn không cần lock”
  Nội dung tham khảo: điều kiện chỉ một producer, một consumer; vì sao
  thêm producer thứ hai thì lập luận không còn đúng.

### W2.3 – Priority inversion và ceiling

- Prompt: “cho tôi hỏi stm32f103c8t6 có set độ ưu tiên lên 8 được không”
  Nội dung tham khảo: phân biệt priority của task FreeRTOS với priority
  của ngắt NVIC.

- Prompt: “sao không làm priority_ceiling.c trước vậy”
  Nội dung tham khảo: mục đích của wrapper và cách so sánh với mutex có
  priority inheritance.

- Prompt: “Dự đoán chained blocking”
  Nội dung tham khảo: vì sao H có thể chờ lần lượt A và B dù mỗi mutex
  đều có inheritance.

- Prompt: “Dự đoán ceiling”
  Nội dung tham khảo: ceiling ngăn vòng chờ do thứ tự lấy khóa ngược
  nhau như thế nào và giới hạn của phép so sánh thời gian block.

- Prompt: “anomaly: trace ended with 1 still switched IN”
  Nội dung tham khảo: cách hiểu cảnh báo cuối file trace khi dump dừng
  giữa lúc một task đang chạy.

### W2.4 – Barrier và notification

- Prompt: “Dự đoán barrier”
  Nội dung tham khảo: ý nghĩa thời gian từ task cuối đến barrier cho tới
  khi các task được giải phóng.

- Prompt: “Dự đoán bug mất sự kiện”
  Nội dung tham khảo: eSetValueWithOverwrite ghi đè giá trị cũ khi task
  gửi nhanh hơn task nhận.

- Prompt: “Dự đoán sau khi sửa”
  Nội dung tham khảo: eIncrement giữ số lần báo, nhưng không giữ riêng
  payload của từng lần gửi.

- Prompt: “Dự đoán RAM/cycle”
  Nội dung tham khảo: phân biệt RAM của event group, semaphore và trạng
  thái notification trong TCB; cách dùng DWT để đo chi phí API.

- Prompt: “# barrier release_cycles= sao đến đây nó ngừng rồi”
  Nội dung tham khảo: xác định nguyên nhân chương trình dừng ở
  configASSERT và thứ tự các task rời barrier.
