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

uint32_t fault_poll(uint32_t control_count, uint32_t imu_count, bool paused, bool valid,
                    int32_t output, uint32_t i2c_count, uint32_t overruns);
void fault_control_done(uint32_t response_us, bool lost);
bool fault_shell_exec(const char *cmd, char *reply, uint32_t size);
void fault_watchdog_start(void);
bool fault_system_ready(uint32_t *errors);

#ifndef W4_MEASURE_STORAGE
#define W4_MEASURE_STORAGE
#endif

extern volatile bool measuring;
extern volatile bool fault_measuring;
void system_rtc_init(void);
bool system_fault_begin(void);
void system_fault_end(void);
bool system_fault_stat(uint32_t role, char *reply, uint32_t size);
extern volatile uint32_t stack_min[5];
extern volatile uint32_t measured_cpu_x100;
void measure_wait(void *task, uint32_t wake, uint32_t current, uint32_t periodic);
void uart_measure_reset(void);
bool uart_line_release(uint32_t *released);
uint32_t uart_stamp_lost(void);
bool system_measure_begin(void);
void system_measure_end(void);
void system_measure_sample(uint32_t elapsed_ms, uint32_t extra_stack_min);
bool system_measure_status(char *reply, uint32_t size);
bool system_log(const char *text);
#endif
