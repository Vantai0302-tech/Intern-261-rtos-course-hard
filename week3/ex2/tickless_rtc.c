#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"
#include "tickless_rtc.h"
#include "params.h"

static const uint32_t rtc_frequency_hz = 32768U;
static uint32_t conversion_remainder;

static uint32_t rtc_read_counter(void)
{
    uint32_t high;
    uint32_t low;

    do {
        high = RTC->CNTH;
        low = RTC->CNTL;
    } while (high != RTC->CNTH);

    return (high << 16U) | low;
}

void tickless_rtc_init(void){
    conversion_remainder = 0U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;

    NVIC_DisableIRQ(RTC_Alarm_IRQn);
    EXTI->IMR &= ~EXTI_IMR_MR17;

    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;
    RCC->BDCR |= RCC_BDCR_LSEON;

    while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0U) {
    }

    RCC->BDCR &= ~RCC_BDCR_RTCSEL;
    RCC->BDCR |= RCC_BDCR_RTCSEL_0;
    RCC->BDCR |= RCC_BDCR_RTCEN;

    RTC->CRL &= ~RTC_CRL_RSF;

    while ((RTC->CRL & RTC_CRL_RSF) == 0U) {
    }

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    RTC->CRH = 0U;

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    RTC->CRL |= RTC_CRL_CNF;

    RTC->PRLH = 0U;
    RTC->PRLL = 0U;

    RTC->CNTH = 0U;
    RTC->CNTL = 0U;

    RTC->ALRH = 0xFFFFU;
    RTC->ALRL = 0xFFFFU;

    RTC->CRL &= ~RTC_CRL_CNF;

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    RTC->CRL &= ~(RTC_CRL_ALRF | RTC_CRL_SECF | RTC_CRL_OWF);

    RTC->CRH = RTC_CRH_ALRIE;

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    EXTI->EMR &= ~EXTI_EMR_MR17;
    EXTI->FTSR &= ~EXTI_FTSR_TR17;
    EXTI->RTSR |= EXTI_RTSR_TR17;

    EXTI->PR = EXTI_PR_PR17;
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);

    NVIC_SetPriority(
        RTC_Alarm_IRQn,
        configLIBRARY_LOWEST_INTERRUPT_PRIORITY);

    EXTI->IMR |= EXTI_IMR_MR17;
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

void RTCAlarm_IRQHandler(void)
{
    RTC->CRL &= ~RTC_CRL_ALRF;
    EXTI->PR = EXTI_PR_PR17;
}

void tickless_suppress_ticks_and_sleep(uint32_t expected_idle_ticks)
{
    const uint32_t cycles_per_tick = configCPU_CLOCK_HZ / configTICK_RATE_HZ;
    const uint32_t wake_early_counts = (rtc_frequency_hz * 2U + 999U) / 1000U;

    if (expected_idle_ticks <= 3U || __get_PRIMASK() != 0U) {
        return;
    }

    if (expected_idle_ticks > configTICK_RATE_HZ * 60U) {
        expected_idle_ticks = configTICK_RATE_HZ * 60U;
    }

    __disable_irq();
    __DSB();
    __ISB();

    uint32_t systick_control = SysTick->CTRL;
    SysTick->CTRL = systick_control & ~SysTick_CTRL_ENABLE_Msk;

    if (eTaskConfirmSleepModeStatus() == eAbortSleep ||
        (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0U) {
        SysTick->CTRL = systick_control;
        __enable_irq();
        return;
    }

    uint32_t remaining_cycles = SysTick->VAL;
    if (remaining_cycles == 0U) {
        remaining_cycles = cycles_per_tick;
    }

    uint32_t rtc_start = rtc_read_counter();
    uint64_t sleep_cycles = remaining_cycles +
        (uint64_t)(expected_idle_ticks - 1U) * cycles_per_tick;
    /* Dậy sớm 2 ms để khôi phục clock. */
    uint32_t sleep_counts = (uint32_t)(
        sleep_cycles * rtc_frequency_hz / configCPU_CLOCK_HZ) - wake_early_counts;
    uint32_t alarm_count = rtc_start + sleep_counts;

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    RTC->CRL &= ~RTC_CRL_ALRF;
    EXTI->PR = EXTI_PR_PR17;
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);

    RTC->CRL |= RTC_CRL_CNF;
    RTC->ALRH = alarm_count >> 16U;
    RTC->ALRL = alarm_count & 0xFFFFU;
    RTC->CRL &= ~RTC_CRL_CNF;

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }

    if ((uint32_t)(rtc_read_counter() - rtc_start) < sleep_counts) {
        PWR->CR &= ~PWR_CR_PDDS;
        PWR->CR |= PWR_CR_LPDS;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        GPIOA->BSRR = GPIO_BSRR_BS1;
        __DSB();
        __WFI();
        GPIOA->BSRR = GPIO_BSRR_BR1;
        __ISB();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

        SystemInit();

        RTC->CRL &= ~RTC_CRL_RSF;
        while ((RTC->CRL & RTC_CRL_RSF) == 0U) {
        }
    }

    uint32_t rtc_elapsed = rtc_read_counter() - rtc_start;
    uint32_t cpu_start = DWT->CYCCNT;
    
    /* Giữ phần dư để không mất thời gian sau mỗi lần quy đổi. */
    sleep_cycles = (uint64_t)rtc_elapsed * configCPU_CLOCK_HZ + conversion_remainder;
    conversion_remainder = (uint32_t)(sleep_cycles % rtc_frequency_hz);
    sleep_cycles = sleep_cycles / rtc_frequency_hz + cycles_per_tick - remaining_cycles;

    uint32_t elapsed_ticks = (uint32_t)(sleep_cycles / cycles_per_tick);
    uint32_t tick_progress = (uint32_t)(sleep_cycles % cycles_per_tick);
    uint32_t step_ticks = elapsed_ticks;

    /* Không nhảy qua deadline của task đang chờ. */
    if (step_ticks >= expected_idle_ticks) {
        step_ticks = expected_idle_ticks - 1U;
    }
    vTaskStepTick((TickType_t)step_ticks);

    for (uint32_t tick = step_ticks; tick < elapsed_ticks; ++tick) {
        (void)xTaskIncrementTick();
    }

    RTC->CRL &= ~RTC_CRL_ALRF;
    EXTI->PR = EXTI_PR_PR17;
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);

    tick_progress += DWT->CYCCNT - cpu_start;
    while (tick_progress >= cycles_per_tick) {
        (void)xTaskIncrementTick();
        tick_progress -= cycles_per_tick;
    }

    SysTick->LOAD = cycles_per_tick - tick_progress - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = systick_control;
    __DSB();
    __ISB();
    SysTick->LOAD = cycles_per_tick - 1U;

    __enable_irq();
}

int32_t tickless_rtc_get_accumulated_drift(void)
{
    /* Sai số quy đổi, tính theo số đếm RTC. */
    return -(int32_t)(conversion_remainder / configCPU_CLOCK_HZ);
}

