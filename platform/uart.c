#include "board.h"
#include "gpio.h"
void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    gpio_mode(GPIOA, 9U, 0xBU);
    gpio_mode(GPIOA, 10U, 4U);
    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->BRR = 625U; /* 72 MHz / 115200, exact integer divisor. */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
void uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = (uint8_t)c;
}
void uart_puts(const char *s) {
     while (*s) { 
        uart_putc(*s++); 
    } 
}
void uart_u32(uint32_t value)
{
    char digits[10]; unsigned n = 0U;
    do { 
        digits[n++] = (char)('0' + value % 10U); 
        value /= 10U; 
    } 
    while (value);
    while (n) {
         uart_putc(digits[--n]);
        }
}
int uart_getc(void)
{
    return (USART1->SR & USART_SR_RXNE) ? (int)(USART1->DR & 255U) : -1;
}
