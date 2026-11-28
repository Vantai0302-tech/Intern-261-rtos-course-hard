#include "FreeRTOS.h"
#include "task.h"
#include "fault_injection.h"
#include "mixed_criticality_system.h"
#include "system_internal.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static uint32_t prev_reason;

int main(void)
{
    board_init();

    fault_injection_init(&prev_reason);
    mixed_criticality_system_init();
    fault_watchdog_start();

    vTaskStartScheduler();

    board_panic(__FILE__, __LINE__);
}