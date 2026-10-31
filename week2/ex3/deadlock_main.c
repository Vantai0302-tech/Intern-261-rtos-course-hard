#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "params.h"
#include "tracer.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

typedef enum {
    LOCK_NOT_STARTED = 0,
    LOCK_HOLDS_FIRST = 1,
    LOCK_WAITS_SECOND = 2,
    LOCK_GOT_BOTH = 3
} lock_progress_t;

static StaticTask_t task_tcb[W2E3_NUM_TASKS];
static StackType_t task_stack[W2E3_NUM_TASKS][256U];
static TaskHandle_t task_handle[W2E3_NUM_TASKS];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticSemaphore_t lock_a_storage;
static StaticSemaphore_t lock_b_storage;
static SemaphoreHandle_t lock_a;
static SemaphoreHandle_t lock_b;
static volatile lock_progress_t low_progress;
static volatile lock_progress_t medium1_progress;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

static BaseType_t tag_low(void *parameter)
{
    (void)parameter;
    return 1;
}

static BaseType_t tag_medium1(void *parameter)
{
    (void)parameter;
    return 4;
}

static BaseType_t tag_medium2(void *parameter)
{
    (void)parameter;
    return 2;
}

static BaseType_t tag_high(void *parameter)
{
    (void)parameter;
    return 3;
}

static void print_progress(const char *label)
{
    uart_puts(label);
    uart_puts(" L=");
    uart_u32((uint32_t)low_progress);
    uart_puts(" M1=");
    uart_u32((uint32_t)medium1_progress);
    uart_puts("\r\n");
}

static void low_task(void *parameter)
{
    (void)parameter;
    BaseType_t taken = xSemaphoreTake(lock_a, portMAX_DELAY);
    configASSERT(taken == pdTRUE);
    low_progress = LOCK_HOLDS_FIRST;

    /* M1 already holds B. Wake it so it tries A before L tries B. */
    xTaskNotifyGive(task_handle[1]);
    low_progress = LOCK_WAITS_SECOND;
    taken = xSemaphoreTake(lock_b, portMAX_DELAY);
    configASSERT(taken == pdTRUE);
    low_progress = LOCK_GOT_BOTH;

    (void)xSemaphoreGive(lock_b);
    (void)xSemaphoreGive(lock_a);
    vTaskSuspend(NULL);
}

static void medium1_task(void *parameter)
{
    (void)parameter;
    BaseType_t taken = xSemaphoreTake(lock_b, portMAX_DELAY);
    configASSERT(taken == pdTRUE);
    medium1_progress = LOCK_HOLDS_FIRST;

    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    medium1_progress = LOCK_WAITS_SECOND;
    taken = xSemaphoreTake(lock_a, portMAX_DELAY);
    configASSERT(taken == pdTRUE);
    medium1_progress = LOCK_GOT_BOTH;

    (void)xSemaphoreGive(lock_a);
    (void)xSemaphoreGive(lock_b);
    vTaskSuspend(NULL);
}

static void medium2_task(void *parameter)
{
    (void)parameter;
    vTaskSuspend(NULL);
}

static void high_observer_task(void *parameter)
{
    (void)parameter;
    vTaskDelay(pdMS_TO_TICKS(100U));
    print_progress("# after_100ms");
    vTaskDelay(pdMS_TO_TICKS(100U));
    print_progress("# after_200ms");
    uart_puts("# progress: 0=not_started 1=holds_first 2=waits_second 3=got_both\r\n");
    uart_puts("# W2E3 deadlock trace: L=1 M2=2 H=3 M1=4\r\n");
    (void)tracer_dump_uart();
    uart_puts("# end\r\n");
    vTaskSuspend(NULL);
}

int main(void)
{
    board_init();
    tracer_init();

    lock_a = xSemaphoreCreateMutexStatic(&lock_a_storage);
    lock_b = xSemaphoreCreateMutexStatic(&lock_b_storage);
    configASSERT(lock_a != NULL && lock_b != NULL);

    task_handle[0] = xTaskCreateStatic(low_task, "tau_L", 256U, NULL,
                                       W2E3_TASK_PRIORITY[0], task_stack[0],
                                       &task_tcb[0]);
    task_handle[1] = xTaskCreateStatic(medium1_task, "tau_M1", 256U, NULL,
                                       W2E3_TASK_PRIORITY[1], task_stack[1],
                                       &task_tcb[1]);
    task_handle[2] = xTaskCreateStatic(medium2_task, "tau_M2", 256U, NULL,
                                       W2E3_TASK_PRIORITY[2], task_stack[2],
                                       &task_tcb[2]);
    task_handle[3] = xTaskCreateStatic(high_observer_task, "tau_H", 256U, NULL,
                                       W2E3_TASK_PRIORITY[3], task_stack[3],
                                       &task_tcb[3]);
    for (uint32_t i = 0U; i < W2E3_NUM_TASKS; ++i) {
        configASSERT(task_handle[i] != NULL);
    }

    vTaskSetApplicationTaskTag(task_handle[0], tag_low);
    vTaskSetApplicationTaskTag(task_handle[1], tag_medium1);
    vTaskSetApplicationTaskTag(task_handle[2], tag_medium2);
    vTaskSetApplicationTaskTag(task_handle[3], tag_high);
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
