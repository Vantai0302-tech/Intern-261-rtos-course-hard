#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_u32(uint32_t value);

/* Returns -1 when no byte is available. */
int uart_getc(void);

#endif /* UART_H */
