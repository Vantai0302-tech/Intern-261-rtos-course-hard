#include "rta_app.h"
#include "FreeRTOS.h"
#include "params.h"
#include "task.h"
#include "tracer.h"
#include "uart.h"
#include "stm32f1xx.h"

static uint32_t task_id[W1E3_NUM_TASKS] = {0U, 1U, 2U, 3U};
static uint32_t tick_zero_cycles;
static uint32_t completed_jobs[W1E3_NUM_TASKS];
static uint32_t max_response_cycles[W1E3_NUM_TASKS];
static uint8_t measuring = 1U;

static void capture_tick_zero(void)
{
    uint32_t tick;
    uint32_t reload;
    uint32_t value;
    uint32_t now;
    uint32_t cycles_per_tick = configCPU_CLOCK_HZ / configTICK_RATE_HZ;

    taskENTER_CRITICAL();
    tick = (uint32_t)xTaskGetTickCount();
    reload = SysTick->LOAD;
    value = SysTick->VAL;
    now = DWT->CYCCNT;
    taskEXIT_CRITICAL();

    configASSERT((reload + 1U) == cycles_per_tick);
    tick_zero_cycles = now - (reload - value) - tick * cycles_per_tick;
}

static void record_response(uint32_t id, TickType_t release_tick, uint32_t done_cycles)
{
    uint32_t cycles_per_tick = configCPU_CLOCK_HZ / configTICK_RATE_HZ;
    uint32_t release_cycles;
    uint32_t response_cycles;

    if (measuring == 0U) {
        return;
    }

    release_cycles = tick_zero_cycles + (uint32_t)release_tick * cycles_per_tick;
    response_cycles = done_cycles - release_cycles;
    ++completed_jobs[id];
    if (response_cycles > max_response_cycles[id]) {
        max_response_cycles[id] = response_cycles;
    }
}

static void dump_response_uart(void)
{
    taskENTER_CRITICAL();
    measuring = 0U;
    taskEXIT_CRITICAL();

    uart_puts("# WCRT: tag,jobs,max_cycles (72 cycles/us)\r\n");
    for (uint32_t i = 0U; i < W1E3_NUM_TASKS; ++i) {
        uart_puts("# ");
        uart_u32(i + 1U);
        uart_putc(',');
        uart_u32(completed_jobs[i]);
        uart_putc(',');
        uart_u32(max_response_cycles[i]);
        uart_puts("\r\n");
    }
}

static BaseType_t task_0_tag(void *parameter)
{
    (void)parameter;
    return 1;
}

static BaseType_t task_1_tag(void *parameter)
{
    (void)parameter;
    return 2;
}

static BaseType_t task_2_tag(void *parameter)
{
    (void)parameter;
    return 3;
}

static BaseType_t task_3_tag(void *parameter)
{
    (void)parameter;
    return 4;
}

static TaskHookFunction_t const task_tags[W1E3_NUM_TASKS] = {
    task_0_tag, task_1_tag, task_2_tag, task_3_tag
};

void vTaskPeriodic(void *pvParameters)
{
    uint32_t id = *(const uint32_t *)pvParameters;
    TickType_t period = pdMS_TO_TICKS(W1E3_PERIOD_MS[id]);
    uint32_t busy_cycles = W1E3_BUSY_WAIT_US[id] * 72U;
    TickType_t last_wake = 0U; 

    if (id == 0U) {
        capture_tick_zero();
    }

    for (;;) {
        uint32_t start = DWT->CYCCNT;

        while ((uint32_t)(DWT->CYCCNT - start) < busy_cycles) {
        }

        record_response(id, last_wake, DWT->CYCCNT);

        if (id == (W1E3_NUM_TASKS - 1U)) {
            int command = uart_getc();
            if ((command == 'd') || (command == 'D')) {
                dump_response_uart();
                uart_puts("# switch trace: cycles,tag,IN|OUT\r\n");
                (void)tracer_dump_uart();
                uart_puts("# end\r\n");
            }
        }

        (void)xTaskDelayUntil(&last_wake, period);
    }
}

void rta_app_init(void)
{
    for (uint32_t i = 0U; i < W1E3_NUM_TASKS; ++i) {
        TaskHandle_t task_handle = NULL;

        BaseType_t status = xTaskCreate(
            vTaskPeriodic,
            "RM_Task",
            192U,
            &task_id[i],
            1U,
            &task_handle
        );

        configASSERT(status == pdPASS);

        vTaskSetApplicationTaskTag(task_handle, task_tags[i]);
        vTaskPrioritySet(task_handle, W1E3_NUM_TASKS - i);
    }
}
