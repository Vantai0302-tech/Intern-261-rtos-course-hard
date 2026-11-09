#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"
#include "gpio.h"
#include "uart.h"
#include "tickless_rtc.h"
#include "params.h"

void board_init(void);

static StaticTask_t test_tcb;
static StackType_t test_stack[256U];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    static StaticTask_t idle_tcb;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

static void test_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t start_tick = last_wake;
    uint32_t next_report = 0U;
    const uint32_t minute_ticks = configTICK_RATE_HZ * 60U;
    const uint32_t test_ticks = minute_ticks * W3E2_DRIFT_TEST_WINDOW_MIN;

    for (;;) {
        uint32_t elapsed_ticks = (uint32_t)(xTaskGetTickCount() - start_tick);

        if (elapsed_ticks >= next_report && next_report <= test_ticks) {
            uart_puts("mcu_ticks=");
            uart_u32(elapsed_ticks);
            uart_puts(" mcu_ms=");
            uart_u32((uint32_t)((uint64_t)elapsed_ticks * 1000U / configTICK_RATE_HZ));
            uart_puts("\r\n");
            next_report += minute_ticks;
        }

        gpio_write(GPIOA, 2U, 1U);
        uint32_t cpu_start = DWT->CYCCNT;
        while ((uint32_t)(DWT->CYCCNT - cpu_start) < configCPU_CLOCK_HZ) {
            __NOP();
        }
        gpio_write(GPIOA, 2U, 0U);

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000U));
    }
}

int main(void)
{
    board_init();
    TIM2->CR1 = 0U;
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;

    gpio_mode(GPIOA, 1U, 2U);
    gpio_mode(GPIOA, 2U, 2U);
    gpio_write(GPIOA, 1U, 0U);
    gpio_write(GPIOA, 2U, 0U);

    tickless_rtc_init();

    TaskHandle_t task = xTaskCreateStatic(test_task, "tickless_test", 256U,
                                        NULL, 1U, test_stack, &test_tcb);
    configASSERT(task != NULL);

    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
