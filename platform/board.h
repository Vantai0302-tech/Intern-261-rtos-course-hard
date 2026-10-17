#ifndef BOARD_H
#define BOARD_H
#include "stm32f1xx.h"
#include <stdint.h>
void board_init(void);
void clock_probe_init(void);
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_u32(uint32_t value);
int uart_getc(void);
void dwt_init(void);
void board_panic(const char *file, unsigned line);
static inline uint32_t cycles_now(void) { return DWT->CYCCNT; }
#endif
