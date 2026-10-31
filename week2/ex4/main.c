#include <FreeRTOS.h>
#include <task.h>
#include <event_groups.h>
#include <semphr.h>
#include <stm32f1xx.h>
#include <sync_app.h>
#include <tracer.h>
#include <uart.h>

void board_init(void);
void board_panic(const char *file, unsigned line);
void sync_app_trace_dump_uart(void);
void sync_app_wait_barrier(void);
uint32_t sync_app_barrier_release_cycles(void);

static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t sender_tcb;
static StackType_t sender_stack[256U];
static StaticTask_t receiver_tcb;
static StackType_t receiver_stack[256U];
static TaskHandle_t sender_handle;
static TaskHandle_t receiver_handle;
static StaticSemaphore_t bench_semaphore_storage;
static SemaphoreHandle_t bench_semaphore;
static volatile uint32_t bench_semaphore_receive_cycles;
static volatile uint32_t bench_notification_receive_cycles;

enum { BENCH_ITERATIONS = 1000U };

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

static BaseType_t sender_tag(void *parameter)
{
    (void)parameter;
    return 1;
}

static BaseType_t receiver_tag(void *parameter)
{
    (void)parameter;
    return 2;
}

static void print_counts(const char *label, uint32_t sent, uint32_t received)
{
    uart_puts(label);
    uart_puts(" sent=");
    uart_u32(sent);
    uart_puts(" received=");
    uart_u32(received);
    uart_puts("\r\n");
}

static void receiver_task(void *parameter)
{
    (void)parameter;
    uint32_t value;

    for (uint32_t i = 0U; i < 500U; ++i) {
        sync_app_notify_receive_buggy(&value);
        BaseType_t ok = xTaskNotifyGive(sender_handle);
        configASSERT(ok == pdPASS);
    }

    for (uint32_t i = 0U; i < 500U; ++i) {
        sync_app_notify_receive_fixed(&value);
        BaseType_t ok = xTaskNotifyGive(sender_handle);
        configASSERT(ok == pdPASS);
    }

    for (uint32_t i = 0U; i < BENCH_ITERATIONS; ++i) {
        uint32_t start = DWT->CYCCNT;
        BaseType_t got = xSemaphoreTake(bench_semaphore, 0U);

        bench_semaphore_receive_cycles += DWT->CYCCNT - start;
        configASSERT(got == pdTRUE);

        BaseType_t ok = xTaskNotifyGive(sender_handle);
        configASSERT(ok == pdPASS);
    }

    for (uint32_t i = 0U; i < BENCH_ITERATIONS; ++i) {
        uint32_t start = DWT->CYCCNT;
        BaseType_t got = xTaskNotifyWait(0U, UINT32_MAX, &value, 0U);

        bench_notification_receive_cycles += DWT->CYCCNT - start;
        configASSERT(got == pdTRUE);
        configASSERT(value == 1U);
        
        BaseType_t ok = xTaskNotifyGive(sender_handle);
        configASSERT(ok == pdPASS);
    }

    for (;;) {
        vTaskSuspend(NULL);
    }
}

static void sender_task(void *parameter)
{
    (void)parameter;
    uint32_t sent_before = 0U;
    uint32_t received_before = 0U;

    sync_app_wait_barrier();

    uart_puts("# barrier release_cycles=");
    uart_u32(sync_app_barrier_release_cycles());
    uart_puts("\r\n");

    uart_puts("# ram event_group_bytes=");
    uart_u32((uint32_t)sizeof(StaticEventGroup_t));
    uart_puts(" binary_semaphore_bytes=");
    uart_u32((uint32_t)sizeof(StaticSemaphore_t));
    uart_puts(" task_tcb_bytes=");
    uart_u32((uint32_t)sizeof(StaticTask_t));
    uart_puts(" stack_word_bytes=");
    uart_u32((uint32_t)sizeof(StackType_t));
    uart_puts("\r\n");

    uart_puts("# W2E4 buggy notification trace\r\n");
    for (uint32_t i = 0U; i < 500U; ++i) {
        sync_app_notify_send_buggy(2U * i + 1U);
        sync_app_notify_send_buggy(2U * i + 2U);
        uint32_t ack = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        configASSERT(ack == 1U);

        if (((i + 1U) % 20U) == 0U) {
            sync_app_trace_dump_uart();
        }
    }
    sync_app_get_counts(&sent_before, &received_before);
    print_counts("# buggy", sent_before, received_before);

    uart_puts("# W2E4 fixed trace: SEND=requested value, RECEIVE=count\r\n");
    for (uint32_t i = 0U; i < 500U; ++i) {
        sync_app_notify_send_fixed(2U * i + 1U);
        sync_app_notify_send_fixed(2U * i + 2U);
        uint32_t ack = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        configASSERT(ack == 1U);

        if (((i + 1U) % 20U) == 0U) {
            sync_app_trace_dump_uart();
        }
    }

    uint32_t sent_after;
    uint32_t received_after;
    sync_app_get_counts(&sent_after, &received_after);
    print_counts("# fixed", sent_after - sent_before,
                 received_after - received_before);

    uart_puts("# W2E4 context-switch trace\r\n");
    (void)tracer_dump_uart();

    uint32_t semaphore_send_cycles = 0U;
    uint32_t notification_send_cycles = 0U;
    for (uint32_t i = 0U; i < BENCH_ITERATIONS; ++i) {
        uint32_t start = DWT->CYCCNT;
        BaseType_t ok = xSemaphoreGive(bench_semaphore);
        semaphore_send_cycles += DWT->CYCCNT - start;
        configASSERT(ok == pdTRUE);
        uint32_t ack = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        configASSERT(ack == 1U);
    }

    for (uint32_t i = 0U; i < BENCH_ITERATIONS; ++i) {
        uint32_t start = DWT->CYCCNT;
        BaseType_t ok = xTaskNotify(receiver_handle, 0U, eIncrement);
        notification_send_cycles += DWT->CYCCNT - start;
        configASSERT(ok == pdPASS);
        uint32_t ack = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        configASSERT(ack == 1U);
    }

    uart_puts("# bench iterations=");
    uart_u32(BENCH_ITERATIONS);
    uart_puts(" semaphore_send_avg_cycles=");
    uart_u32(semaphore_send_cycles / BENCH_ITERATIONS);
    uart_puts(" semaphore_receive_avg_cycles=");
    uart_u32(bench_semaphore_receive_cycles / BENCH_ITERATIONS);
    uart_puts(" notification_send_avg_cycles=");
    uart_u32(notification_send_cycles / BENCH_ITERATIONS);
    uart_puts(" notification_receive_avg_cycles=");
    uart_u32(bench_notification_receive_cycles / BENCH_ITERATIONS);
    uart_puts("\r\n");
    uart_puts("# end\r\n");

    for (;;) {
        vTaskSuspend(NULL);
    }
}

int main(void)
{
    board_init();
    tracer_init();
    sync_app_init_barrier();

    bench_semaphore = xSemaphoreCreateBinaryStatic(&bench_semaphore_storage);
    configASSERT(bench_semaphore != NULL);

    receiver_handle = xTaskCreateStatic(
        receiver_task, "notify_rx", 256U, NULL, 2U,
        receiver_stack, &receiver_tcb);
    sender_handle = xTaskCreateStatic(
        sender_task, "notify_tx", 256U, NULL, 3U,
        sender_stack, &sender_tcb);

    configASSERT(receiver_handle != NULL);
    configASSERT(sender_handle != NULL);
    vTaskSetApplicationTaskTag(receiver_handle, receiver_tag);
    vTaskSetApplicationTaskTag(sender_handle, sender_tag);

    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
