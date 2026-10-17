# W1E1 — Giải thích cấu hình

## Build và kết nối đo

Chạy ở gốc repo: `cmake --preset debug`, sau đó
`cmake --build --preset debug --target w1e1`.
Nạp bằng `cmake --build --preset debug --target w1e1_flash` khi đã cài OpenOCD
và kết nối ST-Link. BOOT0=0; SWDIO=PA13, SWCLK=PA14, chung GND.
USART1: PA9 TX -> RX USB-UART, PA10 RX <- TX USB-UART, 115200 8N1, logic 3.3 V.
Đo PA0 (timer 1 kHz), PA1/PA2 (hai task), PB0 (tag chung), chung GND với logic analyzer.
Đọc và hoàn thiện PREDICTION.md, commit trước khi chạy đo. Chưa có số đo phần cứng.

MSSV 2413032. STM32F103C8T6: HSE 8 MHz, Flash 64 KiB, RAM 20 KiB.

## Linker và startup

MEMORY: Flash 0x08000000..0x0800FFFF, RAM 0x20000000..0x20004FFF.
ENTRY đặt entry ELF; CPU lấy MSP và PC từ vector table. KEEP giữ .isr_vector
khi --gc-sections. .text/.rodata chứa lệnh/hằng; .ARM.extab/.ARM.exidx là metadata ARM.
.data chạy trong RAM nhưng ảnh khởi tạo trong Flash (AT>FLASH); _sidata là nguồn copy,
_sdata/_edata là đích. .bss NOLOAD được xóa bởi startup; .noinit NOLOAD không bị xóa.
ALIGN căn địa chỉ. _estack=0x20005000; stack tăng xuống. .stack giữ 1024 byte MSP/ISR.
ASSERT bắt va chạm với biến RAM. Task stack riêng nằm trong heap/mảng tĩnh.

Startup có 16 vector lõi và 43 IRQ medium-density, reserved=0; IRQ chưa dùng là weak
alias về Default_Handler. Reset copy .data, xóa .bss, gọi SystemInit rồi main.
Không dùng constructor C++; không gọi system/startup mẫu của hãng.

## Thanh ghi đã ghi

| Thanh ghi | Cấu hình và lý do |
|---|---|
| RCC_CR | Bật/chọn HSI trước cấu hình lại; tắt PLL và chờ; bật HSE/chờ HSERDY; bật PLL/chờ PLLRDY |
| FLASH_ACR | PRFTBE=1, LATENCY=2 trước khi HCLK lên 72 MHz |
| RCC_CFGR | PLLSRC=HSE, PLLXTPRE=0, PLLMUL=x9; HPRE=/1, PPRE1=/2, PPRE2=/1, ADCPRE=/6; chọn PLL và kiểm tra SWS |
| RCC_APB2ENR | Bật GPIOA/B/C, AFIO, USART1 |
| RCC_APB1ENR | Bật TIM2 |
| SCB_VTOR | Vector table ở 0x08000000 |
| SCB_AIRCR | Priority grouping=3, configPRIO_BITS=4: cả 4 bit làm preemption priority |
| SCB_SHPR3 | PendSV/SysTick logical priority 15, mã hóa 0xF0; số lớn là ưu tiên thấp |
| GPIO_CRL/CRH | Nibble 0x2: output push-pull 2 MHz; 0xB: alternate push-pull 50 MHz; 0x4: input floating |
| GPIO_BSRR/BRR | Set/reset pin. Toggle đọc ODR trong vùng PRIMASK bảo vệ |
| TIM2_PSC/ARR/CCR1 | 71/999/500 -> 72 MHz/(72*1000)=1 kHz, duty 50% |
| TIM2_CCMR1/CCER | PWM mode 1, preload CCR1, enable CH1 ở PA0 |
| TIM2_EGR/SR/CR1 | UG nạp prescaler, clear status, bật ARPE/CEN |
| USART1_BRR | 625 -> 72 MHz/115200 |
| USART1_CR1/CR2/CR3 | UE/TE/RE, 8 bit, không parity, 1 stop, không flow control |
| USART1_DR | Ghi byte khi TXE; đọc khi RXNE |
| CoreDebug_DEMCR, DWT_CTRL/CYCCNT | Bật TRCENA/CYCCNTENA, zero trước scheduler |

APB1=36 MHz, timer APB1=72 MHz vì prescaler APB1 khác 1. APB2=72 MHz.
FreeRTOS port cấu hình SysTick 1 kHz; SystemCoreClock=72000000.

## FreeRTOS

Max syscall logical priority=5 (0x50). IRQ gọi API FromISR cần logical priority>=5.
PendSV thấp nhất để không đổi task stack giữa một ISR ưu tiên cao hơn.
SVC/PendSV map trực tiếp tới GCC/ARM_CM3 port. Khi bật tracer, SysTick wrapper gọi
handler FreeRTOS rồi trừ phần thân ISR khỏi runtime task; entry/exit còn là overhead.

Hai blinky priority 1, dùng xTaskDelayUntil giữ mốc tuyệt đối. PB0 là chân chung,
PA1/PA2 là probe phụ riêng. Khoảng toggle 379/711 ms, chu kỳ đầy đủ 758/1422 ms.
Khi cùng ready, không dựa vào thứ tự task để đồng bộ. Sai số thực phải đo.

## Tài liệu

- [ST: tài liệu STM32F103, RM0008 và PM0056](https://www.st.com/en/microcontrollers-microprocessors/stm32f103/documentation.html)
- [FreeRTOS ARM_CM3 V11.1.0](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/portable/GCC/ARM_CM3/port.c)
- [GNU ld scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)
