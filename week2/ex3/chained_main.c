#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f1xx.h"

#include "params.h"
#include "tracer.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t task_tcb[W2E3_NUM_TASKS];
static StackType_t task_stack[W2E3_NUM_TASKS][256U];
static TaskHandle_t task_handle[W2E3_NUM_TASKS];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticSemaphore_t lock_a_storage;
static StaticSemaphore_t lock_b_storage;
static SemaphoreHandle_t lock_a;
static SemaphoreHandle_t lock_b;
static UBaseType_t low_effective_priority;
static UBaseType_t medium1_effective_priority;

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

static void busy_wait_ms(uint32_t duration_ms)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = duration_ms * (SystemCoreClock / 1000U);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static void high_task(void *parameter)
{
    (void)parameter;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint32_t start = DWT->CYCCNT;
    BaseType_t taken = xSemaphoreTake(lock_a, portMAX_DELAY);
    uint32_t wait_a_cycles = DWT->CYCCNT - start;
    configASSERT(taken == pdTRUE);
    BaseType_t given = xSemaphoreGive(lock_a);
    configASSERT(given == pdTRUE);

    start = DWT->CYCCNT;
    taken = xSemaphoreTake(lock_b, portMAX_DELAY);
    uint32_t wait_b_cycles = DWT->CYCCNT - start;
    configASSERT(taken == pdTRUE);
    given = xSemaphoreGive(lock_b);
    configASSERT(given == pdTRUE);

    uart_puts("# H wait_A_cycles=");
    uart_u32(wait_a_cycles);
    uart_puts(" wait_B_cycles=");
    uart_u32(wait_b_cycles);
    uart_puts(" total_block_cycles=");
    uart_u32(wait_a_cycles + wait_b_cycles);
    uart_puts("\r\n");
    vTaskSuspend(NULL);
}

static void medium1_task(void *parameter)
{
    (void)parameter;
    BaseType_t taken = xSemaphoreTake(lock_b, portMAX_DELAY);
    configASSERT(taken == pdTRUE);

    /* M1 holds B but waits until H has blocked on B. */
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    medium1_effective_priority = uxTaskPriorityGet(NULL);
    busy_wait_ms(W2E3_LOCK_HOLD_MS_B);

    BaseType_t given = xSemaphoreGive(lock_b);
    configASSERT(given == pdTRUE);
    vTaskSuspend(NULL);
}

static void medium2_task(void *parameter)
{
    (void)parameter;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xTaskNotifyGive(task_handle[1]);
    vTaskSuspend(NULL);
}

static void low_task(void *parameter)
{
    (void)parameter;
    BaseType_t taken = xSemaphoreTake(lock_a, portMAX_DELAY);
    configASSERT(taken == pdTRUE);

    xTaskNotifyGive(task_handle[3]);
    xTaskNotifyGive(task_handle[2]);
    low_effective_priority = uxTaskPriorityGet(NULL);
    busy_wait_ms(W2E3_LOCK_HOLD_MS_A);

    BaseType_t given = xSemaphoreGive(lock_a);
    configASSERT(given == pdTRUE);

    uart_puts("# W2E3 chained trace: L=1 M2=2 H=3 M1=4\r\n");
    uart_puts("# inherited_priority L=");
    uart_u32((uint32_t)low_effective_priority);
    uart_puts(" M1=");
    uart_u32((uint32_t)medium1_effective_priority);
    uart_puts("\r\n");
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
    task_handle[3] = xTaskCreateStatic(high_task, "tau_H", 256U, NULL,
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
