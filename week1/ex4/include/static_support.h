#ifndef W1_STATIC_SUPPORT_H
#define W1_STATIC_SUPPORT_H

#include "FreeRTOS.h"
#include "task.h"

void w1_static_register_task(TaskHandle_t task, const char *name,
                             uint32_t stack_words);
void w1_static_support_init(void);
void w1_static_uart_lock(void);
void w1_static_uart_unlock(void);

#endif
