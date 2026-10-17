#ifndef GPIO_H
#define GPIO_H
#include "stm32f1xx.h"
void gpio_mode(GPIO_TypeDef *port, unsigned pin, unsigned nibble);
void gpio_write(GPIO_TypeDef *port, unsigned pin, unsigned high);
void gpio_toggle(GPIO_TypeDef *port, unsigned pin);
#endif
