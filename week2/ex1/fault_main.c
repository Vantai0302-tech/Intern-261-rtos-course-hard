#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"

#include "i2c1_driver.h"
#include "params.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t fault_tcb;
static StackType_t fault_stack[256U];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static uint32_t recovery_streak;
static volatile uint32_t scl_rising_edges;
static volatile uint32_t first_scl_rise_cycles;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_depth = configMINIMAL_STACK_SIZE;
}

static void log_result(const char *label, i2c1_status_t status)
{
    if (status == I2C1_ERR_RECOVERED) {
        ++recovery_streak;
    } else if (status != I2C1_OK) {
        recovery_streak = 0U;
    }

    uart_puts(label);
    uart_puts(" status=");
    uart_u32((uint32_t)status);
    uart_puts(" recovery_streak=");
    uart_u32(recovery_streak);
    uart_putc('/');
    uart_u32(W2E1_RECOVERY_REPEAT_COUNT);
    uart_puts("\r\n");
}

/* PA1 is open-drain and wired to PB7/SDA only for this test firmware.
   EXTI6 releases it on the first rising SCL edge of bus recovery. */
static void fault_test_init(void)
{
    const uint32_t scl = 1UL << 6U;
    const uint32_t sda_drive = 1UL << 1U;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    GPIOA->BSRR = sda_drive;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << 4U)) | (0x6UL << 4U);

    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & ~(0xFUL << 8U)) | (0x1UL << 8U);
    EXTI->IMR &= ~scl;
    EXTI->RTSR |= scl;
    EXTI->FTSR &= ~scl;
    EXTI->PR = scl;
    NVIC_SetPriority(EXTI9_5_IRQn, 6U);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void EXTI9_5_IRQHandler(void)
{
    const uint32_t scl = 1UL << 6U;
    if ((EXTI->PR & scl) != 0U) {
        EXTI->PR = scl;
        if (scl_rising_edges == 0U) {
            first_scl_rise_cycles = DWT->CYCCNT;
            GPIOA->BSRR = 1UL << 1U;
        }
        ++scl_rising_edges;
    }
}

static void fault_client(void *parameter)
{
    const uint8_t wake[2] = { 0x6BU, 0x00U };
    const uint32_t scl = 1UL << 6U;
    const uint32_t sda = 1UL << 7U;
    (void)parameter;

    for (uint32_t i = 0U; i < W2E1_RECOVERY_REPEAT_COUNT; ++i) {
        configASSERT((GPIOB->IDR & (scl | sda)) == (scl | sda));
        scl_rising_edges = 0U;
        first_scl_rise_cycles = 0U;
        EXTI->PR = scl;
        EXTI->IMR |= scl;
        uint32_t fault_start_cycles = DWT->CYCCNT;
        GPIOA->BRR = 1UL << 1U;
        vTaskDelay(pdMS_TO_TICKS(1U));

        i2c1_status_t fault_status = i2c1_master_write(0x68U, wake, 2U);
        uint32_t recovery_return_cycles = DWT->CYCCNT;
        EXTI->IMR &= ~scl;
        GPIOA->BSRR = 1UL << 1U;
        uint32_t pulses = scl_rising_edges;
        uint32_t first_pulse_cycles = first_scl_rise_cycles;

        i2c1_status_t next_status = I2C1_ERR_BUS;
        uint32_t next_done_cycles = 0U;
        if (fault_status == I2C1_ERR_RECOVERED) {
            next_status = i2c1_master_write(0x68U, wake, 2U);
            next_done_cycles = DWT->CYCCNT;
        }

        log_result("fault", fault_status);
        uart_puts("measure scl_pulses=");
        uart_u32(pulses);
        uart_puts(" fault_to_return_cycles=");
        uart_u32(recovery_return_cycles - fault_start_cycles);
        if (pulses != 0U && next_status == I2C1_OK) {
            uart_puts(" first_pulse_to_next_cycles=");
            uart_u32(next_done_cycles - first_pulse_cycles);
        }
        uart_puts("\r\n");

        if (fault_status != I2C1_ERR_RECOVERED) {
            break;
        }
        log_result("write_after_recovery", next_status);
        if (next_status != I2C1_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void)
{
    board_init();
    i2c1_driver_init();
    fault_test_init();
    uart_puts("# W2E1 GPIO SDA fault test: PA1 wired to PB7/SDA\r\n");

    configASSERT(xTaskCreateStatic(fault_client, "i2c_fault", 256U,
                                   NULL, 2U, fault_stack, &fault_tcb) != NULL);
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
