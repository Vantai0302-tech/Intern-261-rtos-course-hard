#include <FreeRTOS.h>
#include <task.h>
#include <sync_app.h>
#include <tracer.h>
#include <uart.h>

void board_init(void);
void board_panic(const char *file, unsigned line);
void sync_app_trace_dump_uart(void);
void sync_app_wait_barrier(void);

static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t sender_tcb;
static StackType_t sender_stack[256U];
static StaticTask_t receiver_tcb;
static StackType_t receiver_stack[256U];
static TaskHandle_t sender_handle;

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

    TaskHandle_t receiver_handle = xTaskCreateStatic(
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
