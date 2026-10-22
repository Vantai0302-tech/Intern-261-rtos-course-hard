#include<tracer.h>
#include<FreeRTOS.h>
#include<task.h>
#include<uart.h>
#include<gpio.h>
#include<stdbool.h>

#define TRACER_BUFFER_SIZE 256U

static tracer_record_t tracer_buffer[TRACER_BUFFER_SIZE];
static volatile uint32_t head;
static volatile uint32_t tail;
static volatile uint8_t recording;
static volatile uint32_t record_count = 0U;

void tracer_init(void){
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    head = 0U;
    tail = 0U;
    record_count = 0U;
    recording = 1U;

    gpio_mode(GPIOB, 12U, 2U);
    gpio_mode(GPIOB, 13U, 2U);
    gpio_mode(GPIOB, 14U, 2U);

    gpio_write(GPIOB, 12U, 0U);
    gpio_write(GPIOB, 13U, 0U);
    gpio_write(GPIOB, 14U, 0U);
}
static void tracer_toggle_gpio(uint8_t tag){
    switch(tag) {
        case 1U:
        gpio_toggle(GPIOB, 12U);
        break;

        case 2U:
        gpio_toggle(GPIOB, 13U);
        break;

        case 3U:
        gpio_toggle(GPIOB, 14U);
        break;

        default:
        break;
    }

}

static void tracer_record(uint8_t event){
    TaskHookFunction_t tag_function;
    uint32_t timestamp;
    uint8_t tag = 0U;

    if (recording == 0U){
        return;
    }

    tag_function = xTaskGetApplicationTaskTagFromISR(NULL);
    if (tag_function != NULL){
        tag = (uint8_t)tag_function(NULL);

    }
    tracer_toggle_gpio(tag);
    timestamp = DWT->CYCCNT;
    
    tracer_buffer[head].timestamp_cycles = timestamp;
    tracer_buffer[head].task_tag = tag;
    tracer_buffer[head].event = event;

    head = (head + 1U) % TRACER_BUFFER_SIZE;
    if (record_count == TRACER_BUFFER_SIZE){
        tail = (tail + 1U) % TRACER_BUFFER_SIZE;
    } else {
        ++record_count;
    }
}

void tracer_on_switched_in(void){
    tracer_record(0U);
}

void tracer_on_switched_out(void){
    tracer_record(1U);

}

uint32_t tracer_dump_uart(void){
    uint32_t dumped = 0U;

    recording = 0U;

    while (record_count > 0U){
        const tracer_record_t *record = &tracer_buffer[tail];

        uart_u32(record->timestamp_cycles);
        uart_putc(',');
        uart_u32(record->task_tag);
        uart_putc(',');

        if (record->event == 0U){
            uart_puts("IN\r\n");
            
        } 
        else {
            uart_puts("OUT\r\n");
        }

        tail = (tail + 1U) % TRACER_BUFFER_SIZE;
        --record_count;
        ++dumped;

    }
    head = 0U;
    tail = 0U;

    return dumped;
}



