#ifndef W3E1_UART_APP_H
#define W3E1_UART_APP_H

#include "FreeRTOS.h"
#include "params.h"
#include <stdbool.h>
#include <stdint.h>

/* Application helpers; the interface supplied by the course stays in
   uart_dma_rx.h. Echo uses the exact frame format of uart_stress.py. */
typedef struct {
    uint8_t bytes[W3E1_PAYLOAD_LEN_BYTES + 11U];
} uart_echo_frame_t;

typedef struct {
    uint32_t stream_dropped_bytes;
    uint32_t dma_errors;
    uint32_t uart_errors;
    uint32_t late_dma_irqs;
    uint32_t invalid_lengths;
    uint32_t repeated_or_old_sequences;
    uint32_t echo_dropped_frames;
    uint32_t max_stream_used;
} uart_rx_diagnostics_t;

bool uart_dma_rx_get_echo(uart_echo_frame_t *frame, TickType_t wait_ticks);
void uart_dma_rx_get_diagnostics(uart_rx_diagnostics_t *out);
void uart_app_request_report(void);

void uart_runtime_init(void);
uint32_t uart_runtime_counter(void);

#endif
