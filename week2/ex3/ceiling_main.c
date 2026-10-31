#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"

#include "params.h"
#include "priority_ceiling.h"
#include "tracer.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t task_tcb[W2E3_NUM_TASKS];
static StackType_t task_stack[W2E3_NUM_TASKS][256U];
static TaskHandle_t task_handle[W2E3_NUM_TASKS];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static pc_mutex_t lock_a;
static pc_mutex_t lock_b;
static volatile uint32_t low_attempted_a;
static volatile uint32_t low_done;
static volatile uint32_t medium1_done;
static volatile uint32_t high_done;
static uint32_t low_wait_a_cycles;
static uint32_t high_wait_a_cycles;
static uint32_t high_wait_b_cycles;
static UBaseType_t medium1_ceiling_priority;

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

static void low_task(void *parameter)
{
    (void)parameter;
    low_attempted_a = 1U;
    uint32_t start = DWT->CYCCNT;
    pc_mutex_lock(&lock_a);
    low_wait_a_cycles = DWT->CYCCNT - start;
    pc_mutex_lock(&lock_b);
    busy_wait_ms(W2E3_LOCK_HOLD_MS_A);
    low_done = 1U;
    pc_mutex_unlock(&lock_b);
    pc_mutex_unlock(&lock_a);
    vTaskSuspend(NULL);
}

static void medium1_task(void *parameter)
{
    (void)parameter;
    pc_mutex_lock(&lock_b);
    medium1_ceiling_priority = uxTaskPriorityGet(NULL);

    /* B then A is the reverse of L's A then B order. Delay gives L a
       chance to request A while B is owned by M1. */
    vTaskDelay(pdMS_TO_TICKS(2U));
    busy_wait_ms(W2E3_LOCK_HOLD_MS_B);
    pc_mutex_lock(&lock_a);
    medium1_done = 1U;
    pc_mutex_unlock(&lock_a);
    pc_mutex_unlock(&lock_b);
    vTaskSuspend(NULL);
}

static void high_task(void *parameter)
{
    (void)parameter;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint32_t start = DWT->CYCCNT;
    pc_mutex_lock(&lock_a);
    high_wait_a_cycles = DWT->CYCCNT - start;
    pc_mutex_unlock(&lock_a);

    start = DWT->CYCCNT;
    pc_mutex_lock(&lock_b);
    high_wait_b_cycles = DWT->CYCCNT - start;
    pc_mutex_unlock(&lock_b);
    high_done = 1U;
    vTaskSuspend(NULL);
}

static void medium2_observer_task(void *parameter)
{
    (void)parameter;
    vTaskDelay(pdMS_TO_TICKS(1U));
    xTaskNotifyGive(task_handle[3]);
    vTaskDelay(pdMS_TO_TICKS(100U));

    uart_puts("# W2E3 ceiling trace: L=1 M2=2 H=3 M1=4\r\n");
    uart_puts("# completed L=");
    uart_u32(low_done);
    uart_puts(" M1=");
    uart_u32(medium1_done);
    uart_puts(" H=");
    uart_u32(high_done);
    uart_puts(" L_attempted_A=");
    uart_u32(low_attempted_a);
    uart_puts(" M1_ceiling_priority=");
    uart_u32((uint32_t)medium1_ceiling_priority);
    uart_puts("\r\n# H wait_A_cycles=");
    uart_u32(high_wait_a_cycles);
    uart_puts(" wait_B_cycles=");
    uart_u32(high_wait_b_cycles);
    uart_puts(" total_block_cycles=");
    uart_u32(high_wait_a_cycles + high_wait_b_cycles);
    uart_puts("\r\n# L wait_A_cycles=");
    uart_u32(low_wait_a_cycles);
    uart_puts("\r\n");
    (void)tracer_dump_uart();
    uart_puts("# end\r\n");
    vTaskSuspend(NULL);
}

int main(void)
{
    board_init();
    tracer_init();
    pc_mutex_create_static(&lock_a, W2E3_TASK_PRIORITY[3]);
    pc_mutex_create_static(&lock_b, W2E3_TASK_PRIORITY[3]);

    task_handle[0] = xTaskCreateStatic(low_task, "tau_L", 256U, NULL,
                                       W2E3_TASK_PRIORITY[0], task_stack[0],
                                       &task_tcb[0]);
    task_handle[1] = xTaskCreateStatic(medium1_task, "tau_M1", 256U, NULL,
                                       W2E3_TASK_PRIORITY[1], task_stack[1],
                                       &task_tcb[1]);
    task_handle[2] = xTaskCreateStatic(medium2_observer_task, "tau_M2", 256U,
                                       NULL, W2E3_TASK_PRIORITY[2],
                                       task_stack[2], &task_tcb[2]);
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
