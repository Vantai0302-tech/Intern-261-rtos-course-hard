#include "static_support.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "uart.h"

typedef struct {
    TaskHandle_t handle;
    const char *name;
    uint32_t stack_words;
} task_entry_t;

typedef struct {
    uint32_t index;
    uint32_t free_words;
} stack_sample_t;

static task_entry_t tasks[8];
static uint32_t task_count;
static StaticQueue_t sample_queue_control;
static uint8_t sample_queue_storage[8U * sizeof(stack_sample_t)];
static QueueHandle_t sample_queue;
static StaticSemaphore_t uart_mutex_storage;
static SemaphoreHandle_t uart_mutex;
static StaticTask_t sampler_tcb;
static StackType_t sampler_stack[192U];
static StaticTask_t logger_tcb;
static StackType_t logger_stack[256U];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

extern uint8_t _sdata, _edata, _sbss, _ebss, _snoinit, _enoinit;
extern uint8_t _sstack, _estack;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

#if configUSE_TIMERS
static StaticTask_t timer_tcb;
static StackType_t timer_stack[configTIMER_TASK_STACK_DEPTH];
void vApplicationGetTimerTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                    configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &timer_tcb;
    *stack = timer_stack;
    *depth = configTIMER_TASK_STACK_DEPTH;
}
#endif

void w1_static_register_task(TaskHandle_t task, const char *name,
                             uint32_t stack_words)
{
    configASSERT(task != NULL);
    configASSERT(task_count < 8U);
    tasks[task_count].handle = task;
    tasks[task_count].name = name;
    tasks[task_count].stack_words = stack_words;
    ++task_count;
}

void w1_static_uart_lock(void)
{
    BaseType_t ok = xSemaphoreTake(uart_mutex, portMAX_DELAY);
    configASSERT(ok == pdTRUE);
}

void w1_static_uart_unlock(void)
{
    BaseType_t ok = xSemaphoreGive(uart_mutex);
    configASSERT(ok == pdTRUE);
}

static void print_size(const char *name, uintptr_t bytes)
{
    uart_puts(name);
    uart_u32((uint32_t)bytes);
}

static void logger_task(void *parameter)
{
    (void)parameter;
    w1_static_uart_lock();
    uart_puts("# W1.4 static: dynamic_allocation=0; stack units are words\r\n");
    print_size("# ram data=", (uintptr_t)&_edata - (uintptr_t)&_sdata);
    print_size(" bss=", (uintptr_t)&_ebss - (uintptr_t)&_sbss);
    print_size(" noinit=", (uintptr_t)&_enoinit - (uintptr_t)&_snoinit);
    print_size(" main_isr_stack=", (uintptr_t)&_estack - (uintptr_t)&_sstack);
    print_size(" remaining_gap=", (uintptr_t)&_sstack - (uintptr_t)&_enoinit);
    uart_puts("\r\n");
    print_size("# objects tcb_bytes=", sizeof(StaticTask_t));
    print_size(" stack_word_bytes=", sizeof(StackType_t));
    print_size(" queue_control_bytes=", sizeof(sample_queue_control));
    print_size(" queue_buffer_bytes=", sizeof(sample_queue_storage));
    print_size(" mutex_bytes=", sizeof(uart_mutex_storage));
    uart_puts("\r\n");
    w1_static_uart_unlock();

    for (;;) {
        stack_sample_t sample;
        BaseType_t ok = xQueueReceive(sample_queue, &sample, portMAX_DELAY);
        configASSERT(ok == pdTRUE);
        configASSERT(sample.index < task_count);
        task_entry_t *entry = &tasks[sample.index];
        w1_static_uart_lock();
        uart_puts("# stack task=");
        uart_puts(entry->name);
        print_size(" allocated_words=", entry->stack_words);
        print_size(" min_free_words=", sample.free_words);
        print_size(" min_free_bytes=", sample.free_words * sizeof(StackType_t));
        uart_puts("\r\n");
        w1_static_uart_unlock();
    }
}

static void sampler_task(void *parameter)
{
    (void)parameter;
    w1_static_register_task(xTaskGetIdleTaskHandle(), "Idle",
                            configMINIMAL_STACK_SIZE);
#if configUSE_TIMERS
    w1_static_register_task(xTimerGetTimerDaemonTaskHandle(), "Timer",
                            configTIMER_TASK_STACK_DEPTH);
#endif
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000U));
        for (uint32_t i = 0U; i < task_count; ++i) {
            stack_sample_t sample = {
                .index = i,
                .free_words = (uint32_t)uxTaskGetStackHighWaterMark(tasks[i].handle)
            };
            BaseType_t ok = xQueueSend(sample_queue, &sample, portMAX_DELAY);
            configASSERT(ok == pdTRUE);
        }
    }
}

void w1_static_support_init(void)
{
    sample_queue = xQueueCreateStatic(8U, sizeof(stack_sample_t),
                                      sample_queue_storage, &sample_queue_control);
    uart_mutex = xSemaphoreCreateMutexStatic(&uart_mutex_storage);
    configASSERT(sample_queue != NULL);
    configASSERT(uart_mutex != NULL);
    TaskHandle_t sampler = xTaskCreateStatic(sampler_task, "stack_sample", 192U,
                                             NULL, tskIDLE_PRIORITY, sampler_stack,
                                             &sampler_tcb);
    TaskHandle_t logger = xTaskCreateStatic(logger_task, "stack_uart", 256U,
                                            NULL, tskIDLE_PRIORITY, logger_stack,
                                            &logger_tcb);
    w1_static_register_task(sampler, "stack_sample", 192U);
    w1_static_register_task(logger, "stack_uart", 256U);
}
