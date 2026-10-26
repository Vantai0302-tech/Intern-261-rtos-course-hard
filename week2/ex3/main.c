#include<priority_ceiling.h>
#include<stm32f1xx.h>
#include<FreeRTOS.h>
#include<task.h>
#include<semphr.h>
#include<params.h>
#include<tracer.h>
#include<uart.h>

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t task_tcb[W2E3_NUM_TASKS];
static StackType_t task_stack[W2E3_NUM_TASKS][256U];
static TaskHandle_t task_handle[W2E3_NUM_TASKS];

static StaticSemaphore_t lock_a_storage;
static SemaphoreHandle_t lock_a;

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

static BaseType_t tag_low(void *p)     { (void)p; return 1; }
static BaseType_t tag_medium1(void *p) { (void)p; return 4; }
static BaseType_t tag_medium2(void *p) { (void)p; return 2; }
static BaseType_t tag_high(void *p)    { (void)p; return 3; }

static void high_task(void *pvParameter){
    (void)pvParameter;

    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint32_t start = DWT->CYCCNT;
    BaseType_t taken = xSemaphoreTake(lock_a, portMAX_DELAY);

    uint32_t blocked_cycles = DWT->CYCCNT - start;
    configASSERT(taken == pdTRUE);

    BaseType_t given = xSemaphoreGive(lock_a);
    configASSERT(given == pdTRUE);

    uart_puts("# H blocked_cycles =");
    uart_u32(blocked_cycles);
    uart_puts("\r\n");

    for (;;){
        vTaskSuspend(NULL);
    }

}

static void busy_wait_ms(uint32_t duration_ms){
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = duration_ms * (SystemCoreClock / 1000);

    while ((uint32_t)(DWT->CYCCNT - start) < cycles){
        __NOP();
    }
    
}

static void low_task(void *pvParameter){
    (void)pvParameter;

    BaseType_t taken = xSemaphoreTake(lock_a, 0);
    configASSERT(taken == pdTRUE);

    xTaskNotifyGive(task_handle[3]);
    xTaskNotifyGive(task_handle[2]);

    busy_wait_ms(W2E3_LOCK_HOLD_MS_A);

    BaseType_t given = xSemaphoreGive(lock_a);
    configASSERT(given == pdTRUE);

    uart_puts("# W2E3 inversion trace: L=1 M2=2 H=3 M1=4\r\n");
    (void)tracer_dump_uart();
    uart_puts("# end\r\n");

    for (;;){
        vTaskSuspend(NULL);
    }

}

static void medium2_task(void *pvParameter){
    (void)pvParameter;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    busy_wait_ms(200);

    for (;;) {
        vTaskSuspend(NULL);
    }

} 
static void medium1_task(void *pvParameter){
    (void)pvParameter;

    for (;;){
        vTaskSuspend(NULL);
    }

}

int main(void){
    board_init();
    tracer_init();

    lock_a = xSemaphoreCreateBinaryStatic(&lock_a_storage);
    configASSERT(lock_a != NULL);

    BaseType_t given = xSemaphoreGive(lock_a);
    configASSERT(given == pdTRUE);

    task_handle[0] = xTaskCreateStatic (low_task, "tau_L", 256, NULL,
          W2E3_TASK_PRIORITY[0], task_stack[0], &task_tcb[0]);
    task_handle[1] = xTaskCreateStatic (medium1_task, "tau_M1", 256, NULL,
          W2E3_TASK_PRIORITY[1], task_stack[1], &task_tcb[1]);
    task_handle[2] = xTaskCreateStatic (medium2_task, "tau_M2", 256, NULL,
          W2E3_TASK_PRIORITY[2], task_stack[2], &task_tcb[2]);
    task_handle[3] = xTaskCreateStatic (high_task, "tau_H", 256, NULL,
          W2E3_TASK_PRIORITY[3], task_stack[3], &task_tcb[3]);

    for (int32_t i = 0; i < W2E3_NUM_TASKS; i++){
        configASSERT(task_handle[i] != NULL);
    }

    vTaskSetApplicationTaskTag(task_handle[0], tag_low);
    vTaskSetApplicationTaskTag(task_handle[1], tag_medium1);
    vTaskSetApplicationTaskTag(task_handle[2], tag_medium2);
    vTaskSetApplicationTaskTag(task_handle[3], tag_high);
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}







