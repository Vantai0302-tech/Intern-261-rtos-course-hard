#include "heap_experiment.h"
#include "uart.h"

void board_init(void);

int main(void)
{
    board_init();  

    uart_puts("# W1E4 MSSV 2413032\r\n");
    uart_puts("step,free_bytes_now,free_bytes_min_ever\r\n");

    heap_experiment_run();

    uart_puts("# END\r\n");
    for (;;) {
    }
}