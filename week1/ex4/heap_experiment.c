#include<heap_experiment.h>
#include<FreeRTOS.h>
#include<task.h>
#include<params.h>
#include<uart.h>
#include<portable.h>
#include<stdint.h>

static void heap_log(uint32_t step){
    heap_log_entry_t log;

    log.step = step;
    log.free_bytes_now = (uint32_t)xPortGetFreeHeapSize();
    log.free_bytes_min_ever = (uint32_t)xPortGetMinimumEverFreeHeapSize();
    uart_u32(log.step);
    uart_putc(',');
    uart_u32(log.free_bytes_now);
    uart_putc(',');
    uart_u32(log.free_bytes_min_ever);
    uart_puts("\r\n");

    }


void heap_experiment_run(void){
     void *blocks[W1E4_NUM_BLOCKS];
    uint32_t step = 0U;
    for (uint32_t cycle = 0U;
         cycle < W1E4_REPEAT_CYCLES;
         ++cycle) {
        for (uint32_t i = 0U;
             i < W1E4_NUM_BLOCKS;
             ++i) {
            blocks[i] =
                pvPortMalloc(W1E4_BLOCK_SIZE_BYTES[i]);

            configASSERT(blocks[i] != NULL);

            ++step;
            heap_log(step);
        }

        for (uint32_t i = 0U;
             i < W1E4_NUM_BLOCKS;
             ++i) {

            uint32_t index = W1E4_FREE_ORDER[i];

            vPortFree(blocks[index]);
            blocks[index] = NULL;

            ++step;
            heap_log(step);
        }
    }

}