#include "FreeRTOS.h"
#include "task.h"
#include "mixed_criticality_system.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

int main(void)
{
    board_init();
    mixed_criticality_system_init();
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
