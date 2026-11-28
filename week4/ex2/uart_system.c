#include "../../week3/ex1/include/params.h"
#include "../ex1/include/params.h"
#undef W3E1_BAUD_BPS
#define W3E1_BAUD_BPS 921600UL
#include "system_internal.h"
#include "stream_buffer.h"

static size_t rx_send(StreamBufferHandle_t stream, const void *data, size_t length,
                      BaseType_t *woken);

#define xStreamBufferSendFromISR rx_send
#include "../../week3/ex1/uart_dma_rx.c"
#undef xStreamBufferSendFromISR

typedef struct
{
    uint32_t position;
    uint32_t time;
} rx_stamp_t;

static rx_stamp_t rx_stamps[32];
static volatile uint32_t rx_stamp_head;
static volatile uint32_t rx_stamp_tail;
static volatile uint32_t rx_stamp_errors;
static uint32_t rx_sent;
static uint32_t rx_read;
static uint32_t line_time;
static bool line_valid;

static size_t rx_send(StreamBufferHandle_t stream, const void *data, size_t length,
                      BaseType_t *woken)
{
    size_t sent = xStreamBufferSendFromISR(stream, data, length, woken);
    if (measuring || fault_measuring)
    {
        const uint8_t *bytes = data;
        uint32_t stamp = runtime_count();
        for (size_t index = 0U; index < sent; ++index)
        {
            if (bytes[index] == '\r' || bytes[index] == '\n')
            {
                if (rx_stamp_head - rx_stamp_tail < 32U)
                {
                    rx_stamps[rx_stamp_head % 32U].position =
                        rx_sent + (uint32_t)index + 1U;
                    rx_stamps[rx_stamp_head % 32U].time = stamp;
                    __DMB();
                    ++rx_stamp_head;
                }
                else
                {
                    ++rx_stamp_errors;
                }
            }
        }
    }
    rx_sent += (uint32_t)sent;
    return sent;
}

void uart_measure_reset(void)
{
    taskENTER_CRITICAL();
    rx_stamp_head = 0U;
    rx_stamp_tail = 0U;
    rx_stamp_errors = 0U;
    line_valid = false;
    taskEXIT_CRITICAL();
}

bool uart_line_release(uint32_t *released)
{
    *released = line_time;
    return line_valid;
}

uint32_t uart_stamp_lost(void)
{
    return rx_stamp_errors;
}

static TaskHandle_t volatile tx_task;
static volatile bool tx_failed;
static volatile uint32_t tx_errors;

void uart_app_request_report(void)
{
}

void uart_dma_init(void)
{
    uart_dma_rx_init();
    DMA1_Channel4->CCR = 0U;
    DMA1_Channel4->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    NVIC_SetPriority(DMA1_Channel4_IRQn, 6U);
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

int uart_read_byte(TickType_t wait)
{
    uint8_t byte;

    if (xStreamBufferReceive(stream_buffer_handle, &byte, 1U, wait) != 1U)
    {
        return -1;
    }

    ++rx_read;
    if (byte == '\r' || byte == '\n')
    {
        taskENTER_CRITICAL();
        line_valid = false;
        while (rx_stamp_tail != rx_stamp_head)
        {
            rx_stamp_t stamp = rx_stamps[rx_stamp_tail % 32U];
            if ((int32_t)(stamp.position - rx_read) > 0)
            {
                break;
            }
            ++rx_stamp_tail;
            if (stamp.position == rx_read)
            {
                line_time = stamp.time;
                line_valid = true;
                break;
            }
        }
        taskEXIT_CRITICAL();
    }
    return (int)byte;
}

bool uart_dma_send(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0U)
    {
        return false;
    }
    taskENTER_CRITICAL();

    configASSERT(tx_task == NULL);
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    tx_task = xTaskGetCurrentTaskHandle();
    tx_failed = false;
    DMA1_Channel4->CCR = 0U;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    DMA1_Channel4->CMAR = (uint32_t)(uintptr_t)data;
    DMA1_Channel4->CNDTR = length;
    __DMB();
    USART1->CR3 |= USART_CR3_DMAT;
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE |
                         DMA_CCR_PL_1 | DMA_CCR_EN;

    taskEXIT_CRITICAL();

    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30U));

    taskENTER_CRITICAL();

    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    USART1->CR3 &= ~USART_CR3_DMAT;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    tx_task = NULL;
    bool ok = notified != 0U && !tx_failed;

    if (notified == 0U)
    {
        ++tx_errors;
    }

    taskEXIT_CRITICAL();

    return ok;
}

void DMA1_Channel4_IRQHandler(void)
{
    uint32_t flags = DMA1->ISR;

    if ((flags & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) == 0U)
    {
        return;
    }
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    USART1->CR3 &= ~USART_CR3_DMAT;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    BaseType_t woken = pdFALSE;

    if ((flags & DMA_ISR_TEIF4) != 0U)
    {
        tx_failed = true;
        ++tx_errors;
    }
    TaskHandle_t task = tx_task;
    tx_task = NULL;
    if (task != NULL)
    {
        vTaskNotifyGiveFromISR(task, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

bool uart_idle(void)
{
    return tx_task == NULL && (USART1->SR & USART_SR_TC) != 0U &&
           xStreamBufferBytesAvailable(stream_buffer_handle) == 0U &&
           (USART1->SR & USART_SR_RXNE) == 0U;
}

uint32_t uart_errors(void)
{
    return tx_errors + uart_error_count + error_count;
}

uint32_t uart_rx_dropped(void)
{
    return stream_dropped_byte;
}

void uart_pack_frame(uint8_t *frame, const uint8_t *payload, uint32_t sequence)
{
    _Static_assert(W3E1_PAYLOAD_LEN_BYTES == SYSTEM_PAYLOAD_BYTES, "Payload mismatch");
    uint32_t crc = payload_crc(payload, SYSTEM_PAYLOAD_BYTES);
    frame[0] = 0xAAU;
    for (uint32_t byte = 0U; byte < 4U; ++byte)
    {
        frame[1U + byte] = (uint8_t)(sequence >> (byte * 8U));
        frame[7U + SYSTEM_PAYLOAD_BYTES + byte] = (uint8_t)(crc >> (byte * 8U));
    }
    frame[5] = SYSTEM_PAYLOAD_BYTES;
    frame[6] = 0U;
    memcpy(&frame[7], payload, SYSTEM_PAYLOAD_BYTES);
}
