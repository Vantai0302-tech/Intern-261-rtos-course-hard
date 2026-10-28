#include <sync_app.h>
#include <params.h>
#include <FreeRTOS.h>
#include <task.h>
#include <event_groups.h>
#include <stm32f1xx.h>
#include <uart.h>

#define NOTIFY_TRACE_CAPACITY 64U

typedef struct {
    uint32_t cycles;
    uint32_t value;
    uint8_t kind; 
} notify_trace_t;

static StaticEventGroup_t barrier_storage;
static EventGroupHandle_t barrier_group;
static StaticTask_t task_tcb[W2E4_NUM_TASKS];
static StackType_t task_stack[W2E4_NUM_TASKS][192U];

static TaskHandle_t receiver_handle;
static uint32_t total_sent;
static uint32_t total_received;
static notify_trace_t notify_trace[NOTIFY_TRACE_CAPACITY];
static uint32_t notify_trace_count;

static BaseType_t barrier_tag(void *parameter)
{
    (void)parameter;
    return 3;
}

static void register_receiver(void)
{
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    if (receiver_handle == NULL) {
        receiver_handle = self;
    }
    configASSERT(receiver_handle == self);
}

static void record_notify(uint8_t kind, uint32_t value)
{
    uint32_t cycles = DWT->CYCCNT;

    taskENTER_CRITICAL();
    configASSERT(notify_trace_count < NOTIFY_TRACE_CAPACITY);
    notify_trace[notify_trace_count].cycles = cycles;
    notify_trace[notify_trace_count].value = value;
    notify_trace[notify_trace_count].kind = kind;
    ++notify_trace_count;
    taskEXIT_CRITICAL();
}

/* Call between batches, while sender and receiver are not sending/receiving. */
void sync_app_trace_dump_uart(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = notify_trace_count;
    taskEXIT_CRITICAL();

    for (uint32_t i = 0U; i < count; ++i) {
        uart_puts("w2e4,");
        uart_u32(notify_trace[i].cycles);
        uart_putc(',');
        uart_puts(notify_trace[i].kind == 0U ? "SEND," : "RECEIVE,");
        uart_u32(notify_trace[i].value);
        uart_puts("\r\n");
    }

    taskENTER_CRITICAL();
    notify_trace_count = 0U;
    taskEXIT_CRITICAL();
}

void vTaskBarrierParticipant(void *pvParameters)
{
    uint32_t index = (uint32_t)(uintptr_t)pvParameters;
    configASSERT(index < W2E4_NUM_TASKS);

    EventBits_t my_bit = (EventBits_t)(1UL << index);
    EventBits_t result = xEventGroupSync(
        barrier_group,
        my_bit,
        (EventBits_t)W2E4_SYNC_BITS_MASK,
        pdMS_TO_TICKS(W2E4_SYNC_TIMEOUT_MS));

    configASSERT((result & W2E4_SYNC_BITS_MASK) == W2E4_SYNC_BITS_MASK);

    (void)xEventGroupSetBits(barrier_group, my_bit);

    for (;;) {
        vTaskSuspend(NULL);
    }
}

void sync_app_init_barrier(void)
{
    configASSERT(barrier_group == NULL);
    barrier_group = xEventGroupCreateStatic(&barrier_storage);
    configASSERT(barrier_group != NULL);

    for (uint32_t index = 0U; index < W2E4_NUM_TASKS; ++index) {
        TaskHandle_t handle = xTaskCreateStatic(
            vTaskBarrierParticipant,
            "barrier",
            192U,
            (void *)(uintptr_t)index,
            1U,
            task_stack[index],
            &task_tcb[index]);

        configASSERT(handle != NULL);
        vTaskSetApplicationTaskTag(handle, barrier_tag);
    }
}

void sync_app_wait_barrier(void)
{
    EventBits_t bits = xEventGroupWaitBits(
        barrier_group,
        (EventBits_t)W2E4_SYNC_BITS_MASK,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(W2E4_SYNC_TIMEOUT_MS));

    configASSERT((bits & W2E4_SYNC_BITS_MASK) == W2E4_SYNC_BITS_MASK);
}
void sync_app_notify_send_buggy(uint32_t value)
{
    configASSERT(receiver_handle != NULL);
    BaseType_t ok = xTaskNotify(receiver_handle, value, eSetValueWithOverwrite);
    configASSERT(ok == pdPASS);

    record_notify(0U, value);
    taskENTER_CRITICAL();
    ++total_sent;
    taskEXIT_CRITICAL();
}

void sync_app_notify_receive_buggy(uint32_t *out_value)
{
    configASSERT(out_value != NULL);
    register_receiver();

    BaseType_t ok = xTaskNotifyWait(
        0U, UINT32_MAX, out_value, portMAX_DELAY);
    configASSERT(ok == pdTRUE);

    record_notify(1U, *out_value);
    taskENTER_CRITICAL();
    ++total_received;
    taskEXIT_CRITICAL();
}

void sync_app_notify_send_fixed(uint32_t value)
{
    /* eIncrement preserves the count; log the requested payload separately. */
    configASSERT(receiver_handle != NULL);

    BaseType_t ok = xTaskNotify(receiver_handle, 0U, eIncrement);
    configASSERT(ok == pdPASS);

    record_notify(0U, value);
    taskENTER_CRITICAL();
    ++total_sent;
    taskEXIT_CRITICAL();
}

void sync_app_notify_receive_fixed(uint32_t *out_value)
{
    configASSERT(out_value != NULL);
    register_receiver();

    BaseType_t ok = xTaskNotifyWait(
        0U, UINT32_MAX, out_value, portMAX_DELAY);
    configASSERT(ok == pdTRUE);

    record_notify(1U, *out_value);
    taskENTER_CRITICAL();
    total_received += *out_value;
    taskEXIT_CRITICAL();
}

void sync_app_get_counts(uint32_t *sent_count, uint32_t *received_count)
{
    configASSERT(sent_count != NULL);
    configASSERT(received_count != NULL);

    taskENTER_CRITICAL();
    *sent_count = total_sent;
    *received_count = total_received;
    taskEXIT_CRITICAL();
}
