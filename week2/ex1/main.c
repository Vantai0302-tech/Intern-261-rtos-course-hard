#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "i2c1_driver.h"
#include "params.h"
#include "uart.h"

void board_init(void);
void board_panic(const char *file, unsigned line);


static const uint8_t mpu_addr7 = 0x68U;

static StaticTask_t write_tcb;
static StackType_t write_stack[256U];
static StaticTask_t read_tcb;
static StackType_t read_stack[256U];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static StaticSemaphore_t uart_mutex_storage;
static SemaphoreHandle_t uart_mutex;
static uint32_t recovery_streak;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_depth = configMINIMAL_STACK_SIZE;
}

static void log_result(const char *client, i2c1_status_t status,
                       const uint8_t *bytes, uint16_t count)
{
    configASSERT(xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE);

    if (status == I2C1_ERR_RECOVERED) {
        ++recovery_streak;
    } else if (status != I2C1_OK) {
        recovery_streak = 0U;
    }

    uart_puts(client);
    uart_puts(" status=");
    uart_u32((uint32_t)status);
    if (status == I2C1_OK && bytes != NULL) {
        uart_puts(" raw=");
        for (uint16_t i = 0U; i < count; ++i) {
            if (i != 0U) {
                uart_putc(',');
            }
            uart_u32(bytes[i]);
        }
    }
    uart_puts(" recovery_streak=");
    uart_u32(recovery_streak);
    uart_putc('/');
    uart_u32(W2E1_RECOVERY_REPEAT_COUNT);
    uart_puts("\r\n");

    (void)xSemaphoreGive(uart_mutex);
}

static void write_client(void *parameter)
{
    /* PWR_MGMT_1 = 0 wakes an MPU6050 at address 0x68. */
    const uint8_t wake[2] = { 0x6BU, 0x00U };
    (void)parameter;

    for (;;) {
        i2c1_status_t status = i2c1_master_write(mpu_addr7, wake, 2U);
        log_result("write", status, NULL, 0U);
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}

static void read_client(void *parameter)
{
    uint8_t raw[4] = { 0U };
    const uint16_t lengths[3] = { 1U, 2U, 4U };
    uint32_t sample = 0U;
    (void)parameter;

    /* A bare read tests the receive ISR. Its bytes are not identified MPU6050
       registers: register reads require a repeated START transaction. */
    for (;;) {
        uint16_t length = lengths[sample % 3U];
        i2c1_status_t status = i2c1_master_read(mpu_addr7, raw, length);
        log_result("read", status, raw, length);
        ++sample;
        vTaskDelay(pdMS_TO_TICKS(700U));
    }
}
int main(void)
{
    board_init();
    i2c1_driver_init();

    uart_mutex = xSemaphoreCreateMutexStatic(&uart_mutex_storage);
    configASSERT(uart_mutex != NULL);
    uart_puts("# W2E1 I2C clients; raw reads are not register values\r\n");
    configASSERT(xTaskCreateStatic(write_client, "i2c_write", 256U,
                                   NULL, 2U, write_stack, &write_tcb) != NULL);
    configASSERT(xTaskCreateStatic(read_client, "i2c_read", 256U,
                                   NULL, 2U, read_stack, &read_tcb) != NULL);
    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}
