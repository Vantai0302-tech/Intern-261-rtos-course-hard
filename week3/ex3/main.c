#include <FreeRTOS.h>
#include <task.h>
#include <params.h>
#include <uart.h>
#include <fault_postmortem.h>
#include <stdint.h>
#include <stdbool.h>
#include <stm32f1xx.h>

void board_init(void);
void board_panic(const char *file, unsigned line);

void trigger_unaligned_fault(void){
    static uint32_t data[2];
    uint8_t *address = (uint8_t *)data + 1U;

    SCB->SHCSR &= ~SCB_SHCSR_USGFAULTENA_Msk;
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();

    fault_postmortem_arm_canary();

    __asm volatile(
        "ldr r0, [%0]"
        :
        : "r"(address)
        : "r0", "memory"
    );
}

static StaticTask_t fault_task_tcb;
static StackType_t fault_task_stack[256];

static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth){
        *tcb = &idle_tcb;
        *stack = idle_stack;
        *depth = configMINIMAL_STACK_SIZE;
}
static void fault_test_task(void *argument)
{
    (void)argument;
    uart_puts("Press 'u' to trigger UNALIGNED_WORD_ACCESS\r\n");

    for (;;) {
        int command = uart_getc();

        if (command == 'u') {
            uart_puts("Triggering unaligned fault...\r\n");
            while ((USART1->SR & USART_SR_TC) == 0U) {
            }
            trigger_unaligned_fault();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
int main(void){
    fault_record_t record;

    bool has_fault =
        fault_postmortem_check_and_read(&record);

    board_init();

    if (has_fault) {
        fault_postmortem_print(&record);
    }
    TaskHandle_t fault_task = xTaskCreateStatic(
        fault_test_task,
        "FaultTest",
        256U,
        NULL,
        2U,
        fault_task_stack,
        &fault_task_tcb
    );

    configASSERT(fault_task != NULL);

    vTaskStartScheduler();

    board_panic(__FILE__, __LINE__);
    for (;;) {
    }

}


