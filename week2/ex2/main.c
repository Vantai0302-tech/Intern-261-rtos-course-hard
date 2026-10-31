#include<stdint.h>
#include<stdbool.h>
#include<stm32f1xx.h>
#include<FreeRTOS.h>
#include<task.h>
#include<queue.h>
#include<spsc_ring.h>
#include<uart.h>

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t consumer_tcb;
static StackType_t consumer_stack[256U];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}
static spsc_ring_t ring;
static StaticQueue_t queue_control;
static uint8_t queue_storage[W2E2_RING_CAPACITY * sizeof(spsc_elem_t)];
static QueueHandle_t benchmark_queue;
static volatile uint32_t produced = 0U;
typedef enum {
    PHASE_STRESS,
    PHASE_RING_BENCH,
    PHASE_QUEUE_BENCH
} test_phase_t;
static volatile test_phase_t phase = PHASE_STRESS;
static volatile uint32_t bench_sent;
static volatile uint32_t ring_push_cycles;
static volatile uint32_t queue_send_cycles;

static void tim3_stop(void)
{
    TIM3->DIER &= ~TIM_DIER_UIE;
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->SR = 0U;
}

static void tim3_start(void)
{
    TIM3->CNT = 0U;
    TIM3->SR = 0U;
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM3_IRQHandler(void){
    if ((TIM3->SR & TIM_SR_UIF) == 0){
        return;
    }

    TIM3->SR = ~TIM_SR_UIF;
    
    if (phase == PHASE_STRESS) {
        if (produced >= W2E2_STRESS_COUNT) {
            tim3_stop();
            return;
        }

        spsc_elem_t elem = { .seq = produced };
        if (spsc_ring_push_from_isr(&ring, elem)) {
            ++produced;
        }
        return;
    }

    if (bench_sent >= W2E2_BENCH_ITERATIONS) {
        tim3_stop();
        return;
    }

    spsc_elem_t elem = { .seq = bench_sent };
    uint32_t start = DWT->CYCCNT;
    if (phase == PHASE_RING_BENCH) {
        bool sent = spsc_ring_push_from_isr(&ring, elem);
        uint32_t elapsed = DWT->CYCCNT - start;
        if (sent) {
            ring_push_cycles += elapsed;
            ++bench_sent;
        }
    } else {
        BaseType_t task_woken = pdFALSE;
        BaseType_t sent = xQueueSendFromISR(benchmark_queue, &elem,
                                             &task_woken);
        uint32_t elapsed = DWT->CYCCNT - start;
        if (sent == pdPASS) {
            queue_send_cycles += elapsed;
            ++bench_sent;
        }
        portYIELD_FROM_ISR(task_woken);
    }
    if (bench_sent >= W2E2_BENCH_ITERATIONS) {
        tim3_stop();
    }
}

static uint32_t consume_ring_bench(void)
{
    uint32_t received = 0U;
    uint32_t cycles = 0U;
    spsc_elem_t elem;

    while (received < W2E2_BENCH_ITERATIONS) {
        uint32_t start = DWT->CYCCNT;
        bool got = spsc_ring_pop(&ring, &elem);
        uint32_t elapsed = DWT->CYCCNT - start;
        if (got) {
            configASSERT(elem.seq == received);
            cycles += elapsed;
            ++received;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
    }
    return cycles;
}

static uint32_t consume_queue_bench(void)
{
    uint32_t received = 0U;
    uint32_t cycles = 0U;
    spsc_elem_t elem;

    while (received < W2E2_BENCH_ITERATIONS) {
        uint32_t start = DWT->CYCCNT;
        BaseType_t got = xQueueReceive(benchmark_queue, &elem, 0U);
        uint32_t elapsed = DWT->CYCCNT - start;
        if (got == pdPASS) {
            configASSERT(elem.seq == received);
            cycles += elapsed;
            ++received;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
    }
    return cycles;
}

static void consumer_task(void *pvParameters)
{
    uint32_t received = 0U;
    uint32_t errors = 0U;
    uint32_t first_expected = 0U;
    uint32_t first_actual = 0U;
    spsc_elem_t elem;

    (void)pvParameters;

    tim3_start();

    while (received < W2E2_STRESS_COUNT) {
        if (!spsc_ring_pop(&ring, &elem)) {
            /* Producer đã gửi xong mà ring rỗng: không chờ vô hạn. */
            if (produced >= W2E2_STRESS_COUNT) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1U));
            continue;
        }

        if (elem.seq != received) {
            if (errors == 0U) {
                first_expected = received;
                first_actual = elem.seq;
            }
            ++errors;
        }
        ++received;
    }

    uart_puts("received=");
    uart_u32(received);
    uart_puts(" errors=");
    uart_u32(errors);
    uart_puts("\r\n");

    if (errors != 0U) {
        uart_puts("first: expected=");
        uart_u32(first_expected);
        uart_puts(" actual=");
        uart_u32(first_actual);
        uart_puts("\r\n");
    }

    tim3_stop();
    if (received != W2E2_STRESS_COUNT || errors != 0U) {
        vTaskSuspend(NULL);
    }

    /* The stress producer is stopped before head/tail and counters reset. */
    spsc_ring_init(&ring);
    bench_sent = 0U;
    ring_push_cycles = 0U;
    phase = PHASE_RING_BENCH;
    tim3_start();
    uint32_t ring_pop_cycles = consume_ring_bench();
    tim3_stop();

    bench_sent = 0U;
    queue_send_cycles = 0U;
    phase = PHASE_QUEUE_BENCH;
    tim3_start();
    uint32_t queue_receive_cycles = consume_queue_bench();
    tim3_stop();

    uart_puts("bench iterations=");
    uart_u32(W2E2_BENCH_ITERATIONS);
    uart_puts("\r\nring push_avg_cycles=");
    uart_u32(ring_push_cycles / W2E2_BENCH_ITERATIONS);
    uart_puts(" pop_avg_cycles=");
    uart_u32(ring_pop_cycles / W2E2_BENCH_ITERATIONS);
    uart_puts("\r\nqueue send_avg_cycles=");
    uart_u32(queue_send_cycles / W2E2_BENCH_ITERATIONS);
    uart_puts(" receive_avg_cycles=");
    uart_u32(queue_receive_cycles / W2E2_BENCH_ITERATIONS);
    uart_puts("\r\n");

    vTaskSuspend(NULL);
}
static void tim3_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->CR1 = 0U;
    TIM3->PSC = 71U;  /* 72 MHz timer clock -> 1 MHz counter. */
    TIM3->ARR = 99U;  /* 10 kHz update rate. */
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM3_IRQn, 6U);
    NVIC_EnableIRQ(TIM3_IRQn);
}

int main(void)
{
    board_init();
    spsc_ring_init(&ring);
    benchmark_queue = xQueueCreateStatic(
        W2E2_RING_CAPACITY,
        sizeof(spsc_elem_t),
        queue_storage,
        &queue_control);
    configASSERT(benchmark_queue != NULL);
    tim3_init();

    configASSERT(xTaskCreateStatic(consumer_task, "consumer", 256U,
                                   NULL, 2U, consumer_stack,
                                   &consumer_tcb) != NULL);

    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
