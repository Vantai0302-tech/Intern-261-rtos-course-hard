#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f1xx.h"
#include "uart.h"
#include "uart_dma_rx.h"
#include "uart_app.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t parser_tcb, echo_tcb, stats_tcb, idle_tcb;
static StackType_t parser_stack[384U];
static StackType_t echo_stack[256U];
static StackType_t stats_stack[384U];
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticSemaphore_t tx_mutex_storage;
static SemaphoreHandle_t tx_mutex;
static TaskHandle_t stats_handle;
static TaskHandle_t parser_handle;
static TaskHandle_t echo_handle;
static volatile uint32_t runtime_high;

typedef struct {
    uint32_t uptime_ms;
    uint32_t load_x100;
} cpu_sample_t;

/* Retain 12 minutes: the PC script owns the COM port during the test.
   Afterwards '?' requests the stored measurements over the same UART. */
static cpu_sample_t cpu_history[144U];
static uint32_t history_head, history_count;
static TaskStatus_t task_states[8U];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

/* APB1 = 36 MHz, timer clock = 72 MHz. TIM4 is independent of SysTick
   and of TIM2 used by board_init() for the clock probe. */
void uart_runtime_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    TIM4->CR1 = 0U;
    TIM4->DIER = 0U;
    TIM4->PSC = 719U;
    TIM4->ARR = 0xFFFFU;
    TIM4->CNT = 0U;
    runtime_high = 0U;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR = 0U;
    NVIC_SetPriority(TIM4_IRQn, 4U); /* No FreeRTOS calls in this IRQ. */
    NVIC_ClearPendingIRQ(TIM4_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);
    TIM4->DIER = TIM_DIER_UIE;
    TIM4->CR1 = TIM_CR1_CEN;
}

void TIM4_IRQHandler(void)
{
    if ((TIM4->SR & TIM_SR_UIF) != 0U) {
        TIM4->SR = (uint32_t)~TIM_SR_UIF;
        runtime_high += 65536U;
    }
}

uint32_t uart_runtime_counter(void)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    uint32_t high = runtime_high;
    uint32_t low = TIM4->CNT;
    if ((TIM4->SR & TIM_SR_UIF) != 0U) {
        high += 65536U;
        low = TIM4->CNT;
    }
    __set_PRIMASK(saved);
    return high + low;
}

static void tx_lock(void)
{
    BaseType_t ok = xSemaphoreTake(tx_mutex, portMAX_DELAY);
    configASSERT(ok == pdTRUE);
}

static void tx_unlock(void)
{
    BaseType_t ok = xSemaphoreGive(tx_mutex);
    configASSERT(ok == pdTRUE);
}

static void print_value(const char *label, uint32_t value)
{
    uart_puts(label);
    uart_u32(value);
}

static void print_cpu(const cpu_sample_t *sample)
{
    print_value("# cpu uptime_ms=", sample->uptime_ms);
    print_value(" cpu_percent=", sample->load_x100 / 100U);
    uart_putc('.');
    uart_putc((char)('0' + (sample->load_x100 / 10U) % 10U));
    uart_putc((char)('0' + sample->load_x100 % 10U));
    uart_puts("\r\n");
}

static void print_stats(void)
{
    uart_dma_rx_stats_t stats;
    uart_rx_diagnostics_t diag;
    uart_dma_rx_get_stats(&stats);
    uart_dma_rx_get_diagnostics(&diag);
    print_value("# rx ok=", stats.frames_ok);
    print_value(" crc_errors=", stats.frames_crc_error);
    print_value(" seq_lost=", stats.frames_lost);
    print_value(" bytes=", stats.bytes_received);
    uart_puts("\r\n");
    print_value("# errors stream_dropped_bytes=", diag.stream_dropped_bytes);
    print_value(" dma=", diag.dma_errors);
    print_value(" uart=", diag.uart_errors);
    print_value(" late_dma_irqs=", diag.late_dma_irqs);
    print_value(" bad_lengths=", diag.invalid_lengths);
    print_value(" old_sequences=", diag.repeated_or_old_sequences);
    print_value(" echo_dropped=", diag.echo_dropped_frames);
    print_value(" max_stream_used=", diag.max_stream_used);
    uart_puts("\r\n");
}

void uart_app_request_report(void)
{
    if (stats_handle != NULL) {
        (void)xTaskNotifyGive(stats_handle);
    }
}

static void parser_task(void *parameter)
{
    (void)parameter;
    for (;;) {
        (void)uart_dma_rx_parse_once();
    }
}

static void echo_task(void *parameter)
{
    (void)parameter;
    for (;;) {
        uart_echo_frame_t frame;
        if (uart_dma_rx_get_echo(&frame, portMAX_DELAY)) {
            tx_lock();
            for (uint32_t i = 0U; i < sizeof(frame.bytes); ++i) {
                uart_putc((char)frame.bytes[i]);
            }
            tx_unlock();
        }
    }
}

static void stats_task(void *parameter)
{
    (void)parameter;
    uint32_t previous_total = 0U;
    uint32_t previous_idle = 0U;
    TaskHandle_t idle = xTaskGetIdleTaskHandle();

    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000U)) != 0U) {
            tx_lock();
            uart_puts("# report begin; run-time counter unit = 10 us\r\n");
            print_stats();
            uint32_t first = (history_head + 144U - history_count) % 144U;
            for (uint32_t i = 0U; i < history_count; ++i) {
                print_cpu(&cpu_history[(first + i) % 144U]);
            }
            uart_puts("# stack parser=");
            uart_u32((uint32_t)uxTaskGetStackHighWaterMark(parser_handle));
            print_value(" echo=", (uint32_t)uxTaskGetStackHighWaterMark(echo_handle));
            print_value(" stats=", (uint32_t)uxTaskGetStackHighWaterMark(NULL));
            print_value(" idle=", (uint32_t)uxTaskGetStackHighWaterMark(idle));
            uart_puts("\r\n# report end\r\n");
            tx_unlock();
            continue;
        }

        configRUN_TIME_COUNTER_TYPE total;
        UBaseType_t count = uxTaskGetSystemState(task_states, 8U, &total);
        configASSERT(count != 0U);
        uint32_t idle_time = 0U;
        for (UBaseType_t i = 0U; i < count; ++i) {
            if (task_states[i].xHandle == idle) {
                idle_time = (uint32_t)task_states[i].ulRunTimeCounter;
            }
        }
        uint32_t elapsed = (uint32_t)total - previous_total;
        uint32_t idle_elapsed = idle_time - previous_idle;
        previous_total = (uint32_t)total;
        previous_idle = idle_time;
        configASSERT(elapsed != 0U);
        uint32_t active = (idle_elapsed <= elapsed) ? elapsed - idle_elapsed : 0U;
        cpu_sample_t sample = {
            .uptime_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS,
            .load_x100 = (uint32_t)(((uint64_t)active * 10000U) / elapsed)
        };
        cpu_history[history_head] = sample;
        history_head = (history_head + 1U) % 144U;
        if (history_count < 144U) {
            ++history_count;
        }

        tx_lock();
        print_cpu(&sample);
        print_stats();
        for (UBaseType_t i = 0U; i < count; ++i) {
            uart_puts("# runtime task=");
            uart_puts(task_states[i].pcTaskName);
            print_value(" units=", (uint32_t)task_states[i].ulRunTimeCounter);
            uart_puts("\r\n");
        }
        tx_unlock();
    }
}

int main(void)
{
    board_init();
    uart_dma_rx_init();
    tx_mutex = xSemaphoreCreateMutexStatic(&tx_mutex_storage);
    configASSERT(tx_mutex != NULL);
    parser_handle = xTaskCreateStatic(parser_task, "uart_parser", 384U,
                                            NULL, 3U, parser_stack, &parser_tcb);
    echo_handle = xTaskCreateStatic(echo_task, "uart_echo", 256U,
                                          NULL, 2U, echo_stack, &echo_tcb);
    stats_handle = xTaskCreateStatic(stats_task, "uart_stats", 384U,
                                      NULL, 1U, stats_stack, &stats_tcb);
    configASSERT(parser_handle != NULL && echo_handle != NULL && stats_handle != NULL);
    uart_puts("# W3E1 MSSV 2413032; baud=460800 payload=43 rate=26\r\n");
    uart_puts("# Binary echo enabled; send '?' after stress for CPU history\r\n");
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
