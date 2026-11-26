#ifndef W4_SYSTEM_CONFIG_H
#define W4_SYSTEM_CONFIG_H
#include "../../platform/FreeRTOSConfig.h"
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2
#undef configUSE_IDLE_HOOK
#define configUSE_IDLE_HOOK 1
#define configGENERATE_RUN_TIME_STATS 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
void runtime_init(void);
uint32_t runtime_count(void);
void sleep_idle(uint32_t expected_ticks);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() runtime_init()
#define portGET_RUN_TIME_COUNTER_VALUE() runtime_count()
#undef portSUPPRESS_TICKS_AND_SLEEP
#define portSUPPRESS_TICKS_AND_SLEEP(ticks) sleep_idle((uint32_t)(ticks))
#endif
