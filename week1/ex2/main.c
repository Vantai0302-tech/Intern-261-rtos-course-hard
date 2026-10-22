#include "FreeRTOS.h"
#include "task.h"
#include "tracer.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static BaseType_t task_1_tag(void *pvParameter){
    (void)pvParameter;
    return 1;
}

static BaseType_t task_2_tag(void *pvParameter){
    (void)pvParameter;
    return 2;
}

static BaseType_t task_3_tag(void *pvParameter){
    (void)pvParameter;
    return 3;
}
static void task_1(void *parameter)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameter;

    for (;;) {
        (void)xTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(5U));
    }
}

static void task_2(void *parameter)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameter;

    for (;;) {
        (void)xTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(7U));
    }
}

static void task_3(void *parameter)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameter;

    for (;;) {
        int command = uart_getc();

        if ((command == 'd') || (command == 'D')) {
            uart_puts("# trace dump\r\n");
            (void)tracer_dump_uart();
            uart_puts("# end\r\n");
        }

        (void)xTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(11U));
    }
}

int main(void)
{
    TaskHandle_t task_1_handle = NULL;
    TaskHandle_t task_2_handle = NULL;
    TaskHandle_t task_3_handle = NULL;

    board_init();
    tracer_init();

    configASSERT(
        xTaskCreate(
            task_1,
            "trace1",
            192U,
            NULL,
            3U,
            &task_1_handle) == pdPASS);

    configASSERT(
        xTaskCreate(
            task_2,
            "trace2",
            192U,
            NULL,
            2U,
            &task_2_handle) == pdPASS);

    configASSERT(
        xTaskCreate(
            task_3,
            "trace3",
            192U,
            NULL,
            1U,
            &task_3_handle) == pdPASS);

    vTaskSetApplicationTaskTag(task_1_handle, task_1_tag);
    vTaskSetApplicationTaskTag(task_2_handle, task_2_tag);
    vTaskSetApplicationTaskTag(task_3_handle, task_3_tag);

    vTaskStartScheduler();

    board_panic(__FILE__, __LINE__);
}