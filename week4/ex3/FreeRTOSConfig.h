#ifndef W4_FAULT_CONFIG_H
#define W4_FAULT_CONFIG_H
#include "../ex2/FreeRTOSConfig.h"
#undef configMAX_PRIORITIES
#define configMAX_PRIORITIES 10
#define W4_MEASURE_STORAGE __attribute__((section(".noinit")))

void measure_wait(void *task, uint32_t wake, uint32_t current, uint32_t periodic);

#define traceTASK_DELAY_UNTIL(wake)                                                    \
    do                                                                                 \
    {                                                                                  \
        taskENTER_CRITICAL();                                                          \
        measure_wait((void *)pxCurrentTCB,                                             \
                     (uint32_t)(wake),                                                 \
                     (uint32_t)(xTickCount + xPendedTicks),                            \
                     1U);                                                              \
        taskEXIT_CRITICAL();                                                           \
    } while (0)

#define traceTASK_NOTIFY_TAKE_BLOCK(index)                                             \
    do                                                                                 \
    {                                                                                  \
        taskENTER_CRITICAL();                                                          \
        measure_wait((void *)pxCurrentTCB,                                             \
                     (uint32_t)(xTickCount + xTicksToWait),                            \
                     (uint32_t)(xTickCount + xPendedTicks),                            \
                     0U);                                                              \
        taskEXIT_CRITICAL();                                                           \
    } while (0)
#endif
