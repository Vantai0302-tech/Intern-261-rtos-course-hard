#include "gpio.h"
void gpio_mode(GPIO_TypeDef *port, unsigned pin, unsigned nibble)
{
    volatile uint32_t *cr;
    if (pin < 8U) {
        cr = &port->CRL;
    } else {
        cr = &port->CRH;
    }
    unsigned shift = (pin % 8U) * 4U;
    *cr = (*cr & ~(15UL << shift)) | ((nibble & 15UL) << shift);
}
void gpio_write(GPIO_TypeDef *port, unsigned pin, unsigned high)
{
    if (high) { port->BSRR = 1UL << pin; }
    else { port->BRR = 1UL << pin; }
}
void gpio_toggle(GPIO_TypeDef *port, unsigned pin)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    gpio_write(port, pin, (port->ODR & (1UL << pin)) == 0U);
    __set_PRIMASK(saved);
}
