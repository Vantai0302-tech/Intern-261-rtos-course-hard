#ifndef W4_SYSTEM_INTERNAL_H
#define W4_SYSTEM_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

enum
{
    SYSTEM_PAYLOAD_BYTES = 43,
    SYSTEM_FRAME_BYTES = 54
};

enum
{
    TELEMETRY_DUE = 1U,
    SHELL_REPLY_READY = 2U
};

bool i2c_job_start(void);
bool i2c_job_end(void);
bool i2c_fault_pending(void);
bool i2c_busy(void);
bool i2c_recover(void);
void i2c_timeout_irq(void);
void i2c_timer_start(uint32_t timeout_us);
void i2c_timer_stop(void);

void uart_dma_init(void);
int uart_read_byte(TickType_t wait);
bool uart_dma_send(const uint8_t *data, uint16_t length);
bool uart_idle(void);
uint32_t uart_errors(void);
uint32_t uart_rx_dropped(void);
void uart_pack_frame(uint8_t *frame, const uint8_t *payload, uint32_t sequence);

void runtime_init(void);
uint32_t runtime_count(void);
void sleep_idle(uint32_t expected_ticks);
#endif
