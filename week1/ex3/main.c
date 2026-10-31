#include "FreeRTOS.h"
#include "task.h"
#include "tracer.h"
#include "rta_app.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

int main(void)
{
    board_init();
    tracer_init();
    rta_app_init();

    uart_puts("# W1E3 MSSV 2413032\r\n");
    uart_puts("# Send D to dump the captured switch trace\r\n");

    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}

