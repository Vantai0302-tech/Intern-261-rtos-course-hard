#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
#ifndef APP_TRACE
#define APP_TRACE 0
#endif

#if APP_TRACE
void tracer_on_switched_in(void);
void tracer_on_switched_out(void);

#define traceTASK_SWITCHED_IN()  tracer_on_switched_in()
#define traceTASK_SWITCHED_OUT() tracer_on_switched_out()
#endif

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configCPU_CLOCK_HZ 72000000UL
#define configTICK_RATE_HZ 1000U
#define configMAX_PRIORITIES 9
#define configMINIMAL_STACK_SIZE 128U
#define configTOTAL_HEAP_SIZE (8U * 1024U)
#define configMAX_TASK_NAME_LEN 16
#define configUSE_TICKLESS_IDLE 0
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 1
#define configUSE_MUTEXES 1
#define configUSE_TIMERS 0
#define configTIMER_TASK_PRIORITY 1
#define configTIMER_QUEUE_LENGTH 4
#define configTIMER_TASK_STACK_DEPTH 192U
#define configUSE_TRACE_FACILITY 1
#define configUSE_APPLICATION_TASK_TAG 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (15U << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5U << (8U - configPRIO_BITS))
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 1
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

void board_panic(const char *, unsigned);

#define configASSERT(x) \
    do { \
        if ((x) == 0) { \
            __asm volatile ("cpsid i" ::: "memory"); \
            for (;;) {} \
        } \
    } while (0)

#endif
