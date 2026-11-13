#include <FreeRTOS.h>
#include <task.h>
#include <stm32f1xx.h>
#include <uart.h>
#include <fault_postmortem.h>
#include <params.h>
static volatile fault_record_t fault_record
__attribute__((section(".noinit")));

/* Called from your C fault handler (invoked by the naked
 * HardFault_Handler with a pointer to the stacked exception frame on
 * whichever stack - MSP or PSP - was active): fills in a fault_record_t
 * from SCB->CFSR/HFSR/BFAR/MMFAR, pcTaskGetName(xTaskGetCurrentTaskHandle()),
 * and the stacked PC/LR/xPSR, then writes it into the .noinit record
 * (canary assumed already set by the code that triggered the fault). Does
 * not return (halt or controlled reset - your choice, document it). */
void fault_postmortem_capture(const uint32_t *stacked_frame){
    fault_record.stacked_lr = stacked_frame[5];
    fault_record.stacked_pc = stacked_frame[6];
    fault_record.stacked_xpsr = stacked_frame[7];

    uint32_t cfsr = SCB->CFSR;

    fault_record.cfsr = cfsr;
    fault_record.hfsr = SCB->HFSR;
    fault_record.bfar = SCB->BFAR;
    fault_record.mmfar = SCB->MMFAR;

    fault_record.bfar_valid =
        (cfsr & SCB_CFSR_BFARVALID_Msk) != 0U;

    fault_record.mmfar_valid =
        (cfsr & SCB_CFSR_MMARVALID_Msk) != 0U;

    const char *name = "UNKNOWN";

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        TaskHandle_t task = xTaskGetCurrentTaskHandle();

        if (task != NULL) {
            name = pcTaskGetName(task);
        }
    }

    for (uint32_t i = 0U; i < 15U; i++) {
        char c = name[i];
        fault_record.task_name[i] = c;

        if (c == '\0') {
            break;
        }
    }
    fault_record.task_name[15] = '\0';

    __DSB();

    NVIC_SystemReset();

    for (;;) {
        __NOP();
    }
}

/* Call early in main(), before anything else touches the record: returns
 * true if the .noinit record's canary matches W3E3_NOINIT_CANARY (i.e. a
 * valid post-mortem is waiting), in which case *out is filled in. */
bool fault_postmortem_check_and_read(fault_record_t *out){
    if (out == NULL){
        return false;
    }
    if (fault_record.canary !=  W3E3_NOINIT_CANARY){
        return false;
    }

    const volatile uint8_t *src = (const volatile uint8_t *)&fault_record;

    uint8_t *dst = (uint8_t *) out;
    
    for (uint32_t i = 0; i < sizeof(fault_record_t); i++){
        dst[i] = src[i];
    }
    fault_record.canary = 0;
    return true;
}

/* Prints a fault_record_t over UART1 in whatever human-readable format you
 * choose - must include every field needed to run addr2line on
 * stacked_pc and to identify which of W3E3_ASSIGNED_FAULT_NAMES this was. */
static void uart1_putc(char c){
    while ((USART1->SR & USART_SR_TXE) == 0){
    }
    USART1->DR = (uint8_t)c;
}

static void uart1_print(const char *str){
    while (*str != '\0'){
        uart1_putc(*str++);
    }

}

static void uart1_print_hex(uint32_t value){
    const char hex[] = "0123456789ABCDEF";
    
    uart1_print("0x");
    for (int i = 7; i >= 0; i--){
        uint32_t index = (value >> (i * 4)) & 0xFU;
        uart1_putc(hex[index]);
    }
}

void fault_postmortem_print(const fault_record_t *rec){
    if (rec == NULL){
        return;
    }
    uart1_print("CANARY: ");
    uart1_print_hex(rec->canary);

    uart1_print("\r\nPC: ");
    uart1_print_hex(rec->stacked_pc);

    uart1_print("\r\nLR: ");
    uart1_print_hex(rec->stacked_lr);

    uart1_print("\r\nxPSR: ");
    uart1_print_hex(rec->stacked_xpsr);

    uart1_print("\r\nCFSR: ");
    uart1_print_hex(rec->cfsr);

    uart1_print("\r\nHFSR: ");
    uart1_print_hex(rec->hfsr);

    uart1_print("\r\nBFAR: ");
    if (rec->bfar_valid) {
        uart1_print_hex(rec->bfar);
    } else {
        uart1_print("INVALID");
    }

    uart1_print("\r\nMMFAR");
    
    if (rec->mmfar_valid){
        uart1_print_hex(rec->mmfar);
    } else {
        uart1_print("INVALID");
    }
    
    uart1_print("\r\nTASK: ");

    for (uint32_t i = 0; i < sizeof(rec->task_name); i++){
        if (rec->task_name[i] == '\0'){
            break;
        }
        uart1_putc(rec->task_name[i]);
    }

    uart1_print("\r\n============================================\r\n");
}

/* Writes W3E3_NOINIT_CANARY into the record right before you deliberately
 * trigger one of W3E3_ASSIGNED_FAULT_NAMES, so capture() above can later
 * confirm .noinit really did survive the reset. */
void fault_postmortem_arm_canary(void){
    fault_record.canary =  W3E3_NOINIT_CANARY;
    __DSB();
}

__attribute__((naked, noreturn))
void HardFault_Handler(void){
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b fault_postmortem_capture \n"
    );
}