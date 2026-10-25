#include<stdint.h>
#include<stdbool.h>
#include<stm32f1xx.h>
#include<FreeRTOS.h>
#include<task.h>
#include<spsc_ring.h>
#include<uart.h>

void board_init(void);
void board_panic(const char *file, unsigned line);

static StaticTask_t consumer_tcb;
static StackType_t consumer_stack[256U];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}
static spsc_ring_t ring;
static volatile uint32_t produced = 0U;

void TIM3_IRQHandler(void){
    if ((TIM3->SR & TIM_SR_UIF) == 0){
        return;
    }

    TIM3->SR = ~TIM_SR_UIF;
    
    if (produced >= W2E2_STRESS_COUNT){
        TIM3->DIER &= ~TIM_DIER_UIE;
        TIM3->CR1 &= ~TIM_CR1_CEN;
        return;
    }

    spsc_elem_t elem = { .seq = produced };
    if (spsc_ring_push_from_isr (&ring, elem)){
        ++produced;
    }
}

static void consumer_task(void *pvParameters)
{
    uint32_t received = 0U;
    uint32_t errors = 0U;
    uint32_t first_expected = 0U;
    uint32_t first_actual = 0U;
    spsc_elem_t elem;

    (void)pvParameters;

    TIM3->CR1 |= TIM_CR1_CEN;

    while (received < W2E2_STRESS_COUNT) {
        if (!spsc_ring_pop(&ring, &elem)) {
            /* Producer đã gửi xong mà ring rỗng: không chờ vô hạn. */
            if (produced >= W2E2_STRESS_COUNT) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1U));
            continue;
        }

        if (elem.seq != received) {
            if (errors == 0U) {
                first_expected = received;
                first_actual = elem.seq;
            }
            ++errors;
        }
        ++received;
    }

    uart_puts("received=");
    uart_u32(received);
    uart_puts(" errors=");
    uart_u32(errors);
    uart_puts("\r\n");

    if (errors != 0U) {
        uart_puts("first: expected=");
        uart_u32(first_expected);
        uart_puts(" actual=");
        uart_u32(first_actual);
        uart_puts("\r\n");
    }

    vTaskSuspend(NULL);
}
static void tim3_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->CR1 = 0U;
    TIM3->PSC = 71U;  /* 72 MHz timer clock -> 1 MHz counter. */
    TIM3->ARR = 99U;  /* 10 kHz update rate. */
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM3_IRQn, 6U);
    NVIC_EnableIRQ(TIM3_IRQn);
}

int main(void)
{
    board_init();
    spsc_ring_init(&ring);
    tim3_init();

    configASSERT(xTaskCreateStatic(consumer_task, "consumer", 256U,
                                   NULL, 2U, consumer_stack,
                                   &consumer_tcb) != NULL);

    vTaskStartScheduler();
    board_panic(__FILE__, __LINE__);
}