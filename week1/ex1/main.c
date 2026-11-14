#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "blinky.h"
#if APP_STATIC
#include "static_support.h"
#endif

void board_init(void);
void board_panic(const char *file, unsigned line);

int main(void)
{
    board_init();
    uart_puts("# W1E1 MSSV 2413032");
    blinky_app_init();
#if APP_STATIC
    w1_static_support_init();
#endif
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
