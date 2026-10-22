#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "blinky.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

int main(void)
{
    board_init();
    uart_puts("# W1E1 MSSV 2413032");
    blinky_app_init();
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
