#include "board.h"
#include "gpio.h"
#include "FreeRTOS.h"
uint32_t SystemCoreClock = 72000000UL;
static void wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t timeout = 10000000UL; timeout; --timeout) {
        if ((*reg & mask) == value) { return; }
    }
    board_panic(__FILE__, __LINE__);
}
void SystemInit(void)
{
    RCC->CR |= RCC_CR_HSION;
    wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY);
    RCC->CFGR &= ~RCC_CFGR_SW;
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, 0U);
    RCC->CR &= ~RCC_CR_PLLON;
    wait_bits(&RCC->CR, RCC_CR_PLLRDY, 0U);
    RCC->CR |= RCC_CR_HSEON;
    wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY);
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    //HCLK=72, PCLK1=36, PCLK2=72, ADC=12 MHz; HSE directly into PLL. */
    RCC->CFGR = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 |
                RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    RCC->CR |= RCC_CR_PLLON;
    wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY);
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL);
    SCB->VTOR = FLASH_BASE;
    /* Four implemented priority bits, all preemption, no subpriority. */
    NVIC_SetPriorityGrouping(7U - configPRIO_BITS);
    NVIC_SetPriority(PendSV_IRQn, 15U);
    NVIC_SetPriority(SysTick_IRQn, 15U);
    __DSB();
    __ISB();
}
void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
void clock_probe_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    gpio_mode(GPIOA, 0U, 0xBU); /* TIM2_CH1, alternate push-pull 50 MHz. */
    TIM2->CR1 = 0U;
    TIM2->PSC = 71U; /* APB1 timer clock doubles to 72 MHz. */
    TIM2->ARR = 999U;
    TIM2->CCR1 = 500U;
    TIM2->CCMR1 = (6U << 4) | TIM_CCMR1_OC1PE;
    TIM2->CCER = TIM_CCER_CC1E;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}
void board_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                       RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    gpio_mode(GPIOC, 13U, 2U);
    gpio_write(GPIOC, 13U, 1U);
    uart_init();
    dwt_init();
    clock_probe_init();
}
void board_panic(const char *file, unsigned line)
{
    (void)file; (void)line;
    __disable_irq();
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    gpio_mode(GPIOC, 13U, 2U);
    gpio_write(GPIOC, 13U, 0U);
    for (;;) { __NOP(); }
}
