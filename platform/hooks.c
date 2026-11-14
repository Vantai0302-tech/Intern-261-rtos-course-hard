#include "FreeRTOS.h"
#include "task.h"

#if !APP_STATIC
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}
#endif

void vApplicationMallocFailedHook(void)
{
    board_panic(__FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    board_panic(__FILE__, __LINE__);
}
