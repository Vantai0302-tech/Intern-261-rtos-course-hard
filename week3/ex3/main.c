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

static struct {
    StackType_t guard[8U];
    StackType_t stack[256U];
} fault_stack __attribute__((aligned(32)));

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

__attribute__((noinline))
void trigger_invalid_pc_fault(void)
{
    SCB->SHCSR &= ~SCB_SHCSR_USGFAULTENA_Msk;
    __DSB();
    __ISB();
    fault_postmortem_arm_canary();

    __asm volatile(
        "adr.w r0, 1f \n"
        "bic r0, r0, #1 \n"
        "bx r0 \n"
        "1: nop \n"
        :
        :
        : "r0", "memory"
    );
}

static bool protect_memory(uintptr_t address)
{
    /* Board có MPU nhưng header thiết bị không khai báo thanh ghi này. */
    volatile struct {
        uint32_t TYPE;
        uint32_t CTRL;
        uint32_t RNR;
        uint32_t RBAR;
        uint32_t RASR;
    } *mpu = (void *)0xE000ED90UL;
    uint32_t regions = (mpu->TYPE >> 8U) & 0xFFU;

    if (regions == 0U) {
        uart_puts("MPU unavailable; protected-access test cannot run.\r\n");
        return false;
    }

    SCB->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;
    __DMB();
    mpu->CTRL = 0U;
    mpu->RNR = regions - 1U;
    mpu->RBAR = (uint32_t)address;
    /* Vùng 32 byte, AP=0; dùng default map cho địa chỉ còn lại. */
    mpu->RASR = (1UL << 28U) | (4UL << 1U) | 1UL;
    mpu->CTRL = (1UL << 2U) | 1UL;
    __DSB();
    __ISB();
    return true;
}

__attribute__((noinline))
void trigger_null_fault(void)
{
    uintptr_t address = 0U;
    uint32_t value = 1U;

    if (!protect_memory(address)) {
        return;
    }
    fault_postmortem_arm_canary();

    __asm volatile(
        "str %1, [%0]"
        :
        : "r"(address), "r"(value)
        : "memory"
    );
}

__attribute__((noinline))
void trigger_stack_fault(void)
{
    uintptr_t address = (uintptr_t)&fault_stack.guard[7U];
    uint32_t value = 1U;

    if (!protect_memory((uintptr_t)fault_stack.guard)) {
        return;
    }
    fault_postmortem_arm_canary();

    /* Ghi một word ngay dưới biên thấp của stack task. */
    __asm volatile(
        "str %1, [%0]"
        :
        : "r"(address), "r"(value)
        : "memory"
    );
}

static StaticTask_t fault_task_tcb;

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
static void announce_fault(const char *name)
{
    uart_puts("Triggering ");
    uart_puts(name);
    uart_puts("\r\n");
    while ((USART1->SR & USART_SR_TC) == 0U) {
    }
}

static void fault_test_task(void *argument)
{
    (void)argument;
    uart_puts("u=UNALIGNED_WORD_ACCESS, j=INVALID_PC_JUMP\r\n");
    uart_puts("n=NULL_DEREF_WRITE, s=STACK_OVERFLOW_WRITE\r\n");

    for (;;) {
        int command = uart_getc();

        switch (command) {
        case 'u':
            announce_fault("UNALIGNED_WORD_ACCESS");
            trigger_unaligned_fault();
            break;
        case 'j':
            announce_fault("INVALID_PC_JUMP");
            trigger_invalid_pc_fault();
            break;
        case 'n':
            announce_fault("NULL_DEREF_WRITE");
            trigger_null_fault();
            break;
        case 's':
            announce_fault("STACK_OVERFLOW_WRITE");
            trigger_stack_fault();
            break;
        default:
            break;
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
        fault_stack.stack,
        &fault_task_tcb
    );

    configASSERT(fault_task != NULL);

    vTaskStartScheduler();

    board_panic(__FILE__, __LINE__);
    for (;;) {
    }

}
