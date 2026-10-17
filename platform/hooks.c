#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

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