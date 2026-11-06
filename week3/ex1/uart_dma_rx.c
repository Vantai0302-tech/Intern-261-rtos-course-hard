#include<uart_dma_rx.h>
#include<FreeRTOS.h>
#include<params.h>
#include<task.h>
#include<stream_buffer.h>
#include<stm32f1xx.h>
#include "queue.h"
#include "uart_app.h"
#include <string.h>

#define DMA_RX_SIZE 256U
#define STREAM_RX_SIZE 512U
#define PCLK2_HZ 72000000UL

typedef enum {
    PARSE_SYNC,
    PARSE_HEADER,
    PARSE_PAYLOAD,
    PARSE_CRC
} parser_state_t;


static uint8_t dma_rx_buffer[DMA_RX_SIZE];
static StaticStreamBuffer_t stream_buffer_control;
static uint8_t stream_buffer_storage[STREAM_RX_SIZE + 1U];
static  StreamBufferHandle_t stream_buffer_handle;
static uint32_t stream_dropped_byte;

static uint16_t dma_position = 0U;
static uart_dma_rx_stats_t rx_stats = {0};
static uint32_t error_count;
static uint32_t uart_error_count;
static uint32_t late_dma_irq_count;
static uint32_t invalid_length_count;
static uint32_t old_sequence_count;
static uint32_t echo_dropped_frames;
static uint32_t max_stream_used;
static StaticQueue_t echo_queue_control;
static uint8_t echo_queue_storage[8U * sizeof(uart_echo_frame_t)];
static QueueHandle_t echo_queue;

void uart_dma_rx_init(void){

    configASSERT(stream_buffer_handle == NULL);

    stream_buffer_handle = xStreamBufferCreateStatic(
        sizeof(stream_buffer_storage),
        1U,
        stream_buffer_storage,
        &stream_buffer_control
    );
    configASSERT(stream_buffer_handle != NULL);

    echo_queue = xQueueCreateStatic(8U, sizeof(uart_echo_frame_t),
                                    echo_queue_storage, &echo_queue_control);
    configASSERT(echo_queue != NULL);

    dma_position = 0U;
    
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    GPIOA->CRH &= ~(0xFU << 8U);
    GPIOA->CRH |= 0x4 << 8U;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL << 4U)) | (0xBUL << 4U);

    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;

    USART1->BRR = (PCLK2_HZ + W3E1_BAUD_BPS / 2U) / W3E1_BAUD_BPS;

    USART1->CR1 = USART_CR1_RE | USART_CR1_TE;
    
    // Cấu hình DMA

    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)(uintptr_t)dma_rx_buffer;
    DMA1_Channel5->CNDTR = DMA_RX_SIZE;
    
    DMA1_Channel5->CCR = DMA_CCR_MINC
                    | DMA_CCR_CIRC
                    | DMA_CCR_HTIE
                    | DMA_CCR_TCIE
                    | DMA_CCR_TEIE
                    | DMA_CCR_PL_1;


    // Xóa cờ DMA cũ của channel5
    DMA1->IFCR = DMA_IFCR_CGIF5;

    // Bật ngắt USART IDLE
    USART1->CR1 |= USART_CR1_IDLEIE;

    volatile uint32_t dummy;
    dummy = USART1->SR;
    dummy = USART1->DR;
    (void)dummy;

    NVIC_SetPriority(DMA1_Channel5_IRQn, 6U);
    NVIC_SetPriority(USART1_IRQn, 6U);

    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
    NVIC_ClearPendingIRQ(USART1_IRQn);

    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);

    DMA1_Channel5->CCR |= DMA_CCR_EN;
    USART1->CR3 |= USART_CR3_DMAR | USART_CR3_EIE;
    
    USART1->CR1 |= USART_CR1_UE;

}

static void dma_rx_process(BaseType_t *task_woken){
    uint16_t position = (uint16_t)(DMA_RX_SIZE - DMA1_Channel5->CNDTR);

    if (position == DMA_RX_SIZE){
        position = 0U;
    }

    uint16_t length = (uint16_t)((position + DMA_RX_SIZE - dma_position) % DMA_RX_SIZE);
    if (length == 0U) {
        return ;
    }
    // Vị trí cú đến vị trí mới
    size_t first = DMA_RX_SIZE - dma_position;
    if (first > length){
        first = length;
    }

    __DMB();

    size_t sent = xStreamBufferSendFromISR(
        stream_buffer_handle,
        &dma_rx_buffer[dma_position],
        first,
        task_woken);

    stream_dropped_byte += (uint32_t)(first - sent);
    
    // Nếu DMA đã gửi vòng về đầu, gửi tiếp đoạn đầu buffer
    size_t second = length - first;
    if (second != 0U){
        sent = xStreamBufferSendFromISR(
            stream_buffer_handle,
            dma_rx_buffer,
            second,
            task_woken);

        stream_dropped_byte += (uint32_t)(second - sent);
    }
    rx_stats.bytes_received += length;
    dma_position = position;
    uint32_t used = (uint32_t)xStreamBufferBytesAvailable(stream_buffer_handle);
    if (used > max_stream_used) {
        max_stream_used = used;
    }

}

void USART1_IRQHandler(void){
    uart_dma_rx_on_usart_irq();
}

void uart_dma_rx_on_usart_irq(void){
    uint32_t status = USART1->SR;
    BaseType_t task_woken = pdFALSE;

    const uint32_t errors = USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE;
    if ((status & (USART_SR_IDLE | errors)) != 0U){
        (void)USART1->DR;

        if ((status & errors) != 0U) {
            ++uart_error_count;
        }

        dma_rx_process(&task_woken);
    }
    portYIELD_FROM_ISR(task_woken);

}
void uart_dma_rx_on_dma_irq(void){
    uint32_t status = DMA1->ISR;
    BaseType_t task_woken = pdFALSE;

    if ((status & DMA_ISR_TEIF5) != 0){
        DMA1_Channel5->CCR &= ~DMA_CCR_EN;
        USART1->CR3 &= ~USART_CR3_DMAR;
        DMA1->IFCR = DMA_IFCR_CGIF5;
        ++error_count;
        return;
    }

    uint32_t clear_flags = 0U;
    /* Both flags pending indicate service later than a half-buffer period.
       Position alone cannot reconstruct complete missed DMA laps. */
    if ((status & (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)) ==
        (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)) {
        ++late_dma_irq_count;
    }
    if ((status & DMA_ISR_HTIF5) != 0U){
        clear_flags |= DMA_IFCR_CHTIF5;
    }
    if ((status & DMA_ISR_TCIF5) != 0U){
        clear_flags |= DMA_IFCR_CTCIF5;
    }

    if (clear_flags != 0U){
        DMA1->IFCR = clear_flags;
        dma_rx_process(&task_woken);
    }
    portYIELD_FROM_ISR(task_woken);
}

void DMA1_Channel5_IRQHandler(void){
    uart_dma_rx_on_dma_irq();
}

static uint32_t read_value(const uint8_t *data){
     return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8U)
        | ((uint32_t)data[2] << 16U)
        | ((uint32_t)data[3] << 24U);
}

static uint32_t payload_crc(const uint8_t *data, uint16_t length){
     uint32_t crc = 0xFFFFFFFFUL;

    for (uint16_t i = 0U; i < length; ++i) {
        crc ^= data[i];

        for (uint32_t bit = 0U; bit < 8U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;

}
bool uart_dma_rx_parse_once(void){
      static parser_state_t state = PARSE_SYNC;
    static uint8_t header[6U];
    static uint8_t payload[W3E1_PAYLOAD_LEN_BYTES];
    static uint8_t crc_bytes[4U];
    static uint16_t index;
    static uint32_t expected_seq;

    uint8_t byte;

    size_t received = xStreamBufferReceive(
        stream_buffer_handle,
        &byte,
        1U,
        portMAX_DELAY);

    if (received != 1U) {
        return false;
    }

    switch (state) {
    case PARSE_SYNC:

        if (byte == 0xAAU) {
            index = 0U;
            state = PARSE_HEADER;
        } else if (byte == '?') {
            uart_app_request_report();
        }
        break;

    case PARSE_HEADER:

        header[index++] = byte;

        if (index == sizeof(header)) {
            uint16_t length = (uint16_t)(
                (uint16_t)header[4]
                | ((uint16_t)header[5] << 8U));

            index = 0U;

            if (length == W3E1_PAYLOAD_LEN_BYTES) {
                state = PARSE_PAYLOAD;
            } else {
                ++invalid_length_count;
                state = PARSE_SYNC;
            }
        }
        break;

    case PARSE_PAYLOAD:

        payload[index++] = byte;

        if (index == sizeof(payload)) {
            index = 0U;
            state = PARSE_CRC;
        }
        break;

    case PARSE_CRC:
        crc_bytes[index++] = byte;

        if (index == sizeof(crc_bytes)) {
            uint32_t seq = read_value(header);
            uint32_t received_crc = read_value(crc_bytes);
            uint32_t calculated_crc =
                payload_crc(payload, sizeof(payload));

            taskENTER_CRITICAL();

            if (received_crc == calculated_crc) {
                ++rx_stats.frames_ok;
            } else {
                ++rx_stats.frames_crc_error;
            }

            uint32_t gap = seq - expected_seq;

            if (gap < 0x80000000UL) {
                rx_stats.frames_lost += gap;
                expected_seq = seq + 1U;
            } else {
                ++old_sequence_count;
            }

            taskEXIT_CRITICAL();

            uart_echo_frame_t frame;
            frame.bytes[0] = 0xAAU;
            memcpy(&frame.bytes[1], header, sizeof(header));
            memcpy(&frame.bytes[7], payload, sizeof(payload));
            memcpy(&frame.bytes[7U + sizeof(payload)], crc_bytes, sizeof(crc_bytes));
            if (xQueueSend(echo_queue, &frame, 0U) != pdPASS) {
                ++echo_dropped_frames;
            }

            index = 0U;
            state = PARSE_SYNC;
            return true;
        }
        break;

    default:
        index = 0U;
        state = PARSE_SYNC;
        break;
    }

    return false;
   
}

void uart_dma_rx_get_stats(uart_dma_rx_stats_t *out){
    configASSERT(out != NULL);
    taskENTER_CRITICAL();
    *out = rx_stats;
    taskEXIT_CRITICAL();
}

bool uart_dma_rx_get_echo(uart_echo_frame_t *frame, TickType_t wait_ticks)
{
    configASSERT(frame != NULL);
    return xQueueReceive(echo_queue, frame, wait_ticks) == pdPASS;
}

void uart_dma_rx_get_diagnostics(uart_rx_diagnostics_t *out)
{
    configASSERT(out != NULL);
    taskENTER_CRITICAL();
    out->stream_dropped_bytes = stream_dropped_byte;
    out->dma_errors = error_count;
    out->uart_errors = uart_error_count;
    out->late_dma_irqs = late_dma_irq_count;
    out->invalid_lengths = invalid_length_count;
    out->repeated_or_old_sequences = old_sequence_count;
    out->echo_dropped_frames = echo_dropped_frames;
    out->max_stream_used = max_stream_used;
    taskEXIT_CRITICAL();
}
