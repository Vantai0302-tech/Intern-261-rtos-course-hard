#include "../../week2/ex1/include/params.h"
#include "system_internal.h"
#include <i2c1_driver.h>
#include <params.h>
#include <stm32f1xx.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

typedef enum
{
    STATE_IDLE = 0,
    STATE_START,
    STATE_ADDR,
    STATE_TX,
    STATE_RX,
    STATE_WAIT_BTF,
    STATE_DONE,
    STATE_ERROR
} i2c_state_t;

typedef struct
{
    volatile i2c_state_t state;
    uint8_t addr7;
    bool is_read;
    const uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    uint16_t length;
    volatile uint16_t index;
    TaskHandle_t volatile waiting_task;
    volatile i2c1_status_t status;
    volatile uint32_t error_flags;
} i2c_transaction_t;

static i2c_transaction_t transaction;

static StaticSemaphore_t i2c_mutex_storage;
static SemaphoreHandle_t i2c_mutex;

static volatile bool recovery_pending;
static volatile bool job_expired;
static volatile bool job_open;
static TaskHandle_t job_task;

static void cancel_transfer(void)
{
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    if ((I2C1->SR2 & I2C_SR2_MSL) != 0U)
    {
        I2C1->CR1 |= I2C_CR1_STOP;
    }
    I2C1->CR1 &= ~I2C_CR1_PE;
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    transaction.waiting_task = NULL;
    recovery_pending = true;
}

bool i2c_job_start(void)
{
    bool ready;

    taskENTER_CRITICAL();

    ready = !recovery_pending && !job_open;
    if (ready)
    {
        job_open = true;
        job_expired = false;
        job_task = xTaskGetCurrentTaskHandle();
        (void)ulTaskNotifyTake(pdTRUE, 0U);
        i2c_timer_start(1500U);
    }

    taskEXIT_CRITICAL();

    return ready;
}

bool i2c_job_end(void)
{
    taskENTER_CRITICAL();

    i2c_timer_stop();
    bool timely = !job_expired;
    job_open = false;
    job_task = NULL;
    (void)ulTaskNotifyTake(pdTRUE, 0U);

    taskEXIT_CRITICAL();

    return timely;
}

bool i2c_fault_pending(void)
{
    return recovery_pending;
}

bool i2c_busy(void)
{
    return job_open || recovery_pending || (I2C1->SR2 & I2C_SR2_BUSY) != 0U;
}

void i2c_timeout_irq(void)
{
    if (!job_open)
    {
        return;
    }
    BaseType_t woken = pdFALSE;
    job_expired = true;
    transaction.status = I2C1_ERR_TIMEOUT;
    transaction.state = STATE_ERROR;
    cancel_transfer();
    if (job_task != NULL)
    {
        vTaskNotifyGiveFromISR(job_task, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

bool i2c_recover(void)
{
    if (!recovery_pending || job_open || xSemaphoreTake(i2c_mutex, 0U) != pdTRUE)
    {
        return false;
    }
    bool recovered = i2c1_bus_recover();

    taskENTER_CRITICAL();

    (void)xSemaphoreGive(i2c_mutex);
    recovery_pending = !recovered;

    taskEXIT_CRITICAL();

    return recovered;
}

void i2c1_driver_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->CRL &= ~((0xFU << 24U) | (0xFU << 28U));
    GPIOB->CRL |= ((0xFU << 24U) | (0xFU << 28U));

    I2C1->CR1 &= ~I2C_CR1_PE;

    I2C1->CR2 = 36U;

    I2C1->CCR = 180U;
    I2C1->TRISE = 37U;

    I2C1->CR1 |= I2C_CR1_PE;

    i2c_mutex = xSemaphoreCreateMutexStatic(&i2c_mutex_storage);
    configASSERT(i2c_mutex != NULL);
    NVIC_SetPriority(I2C1_EV_IRQn, 6U);
    NVIC_SetPriority(I2C1_ER_IRQn, 6U);

    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN;

    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

static i2c1_status_t
transfer(uint8_t addr7, const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    if (addr7 > 0x7FU || length == 0U || (tx == NULL && rx == NULL))
    {
        return I2C1_ERR_BUS;
    }
    if (!job_open || job_expired || recovery_pending ||
        job_task != xTaskGetCurrentTaskHandle() ||
        xSemaphoreTake(i2c_mutex, 0U) != pdTRUE)
    {
        return I2C1_ERR_BUS;
    }
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0U && !job_expired)
    {
        __NOP();
    }

    taskENTER_CRITICAL();

    if (job_expired || (I2C1->SR2 & I2C_SR2_BUSY) != 0U)
    {
        cancel_transfer();

        taskEXIT_CRITICAL();
        (void)xSemaphoreGive(i2c_mutex);
        return I2C1_ERR_TIMEOUT;
    }
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    transaction.addr7 = addr7;
    transaction.is_read = rx != NULL;
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;
    transaction.length = length;
    transaction.index = 0U;
    transaction.waiting_task = job_task;
    transaction.status = I2C1_ERR_BUS;
    transaction.error_flags = 0U;
    transaction.state = STATE_START;
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
    if (rx != NULL && length == 2U)
    {
        I2C1->CR1 |= I2C_CR1_POS;
    }
    __DMB();
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN;
    I2C1->CR1 |= I2C_CR1_START;

    taskEXIT_CRITICAL();

    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    taskENTER_CRITICAL();

    i2c1_status_t result = job_expired ? I2C1_ERR_TIMEOUT : transaction.status;
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    if (result != I2C1_OK)
    {
        cancel_transfer();
    }
    transaction.waiting_task = NULL;
    transaction.state = STATE_IDLE;
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);

    taskEXIT_CRITICAL();
    (void)xSemaphoreGive(i2c_mutex);

    return result;
}

i2c1_status_t i2c1_master_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    return transfer(addr7, data, NULL, len);
}

i2c1_status_t i2c1_master_read(uint8_t addr7, uint8_t *data, uint16_t len)
{
    return transfer(addr7, NULL, data, len);
}

static void recovery_delay(void)
{
    uint32_t cycles = SystemCoreClock / (2UL * W2E1_I2C_SPEED_HZ);
    uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
        __NOP();
    }
}

bool i2c1_bus_recover(void)
{
    const uint32_t scl = 1UL << 6U;
    const uint32_t sda = 1UL << 7U;
    const uint32_t pin_mask = (0xFUL << 24U) | (0xFUL << 28U);

    uint32_t save_cr2 = I2C1->CR2;
    uint32_t save_crr = I2C1->CCR;
    uint32_t save_trise = I2C1->TRISE;

    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);

    I2C1->CR1 &= ~I2C_CR1_PE;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    GPIOB->BSRR = scl | sda;
    GPIOB->CRL = (GPIOB->CRL & ~pin_mask) | (0x7UL << 24U) | (0x7UL << 28U);

    recovery_delay();

    for (uint32_t pulse = 0U; pulse < 9U; ++pulse)
    {
        if (pulse != 0U && (GPIOB->IDR & sda) != 0U)
        {
            break; //Slave đã nhả sda
        }
        GPIOB->BRR = scl;
        recovery_delay();
        GPIOB->BSRR = scl;
        recovery_delay();

        if ((GPIOB->IDR & scl) == 0U)
        {
            break;
        }
    }

    if ((GPIOB->IDR & (scl | sda)) == (scl | sda))
    {
        /* SDA xuống rồi lên khi SCL cao: START rồi STOP. */
        GPIOB->BRR = sda;
        recovery_delay();
        GPIOB->BSRR = sda;
        recovery_delay();
    }

    bool lines_high = (GPIOB->IDR & (scl | sda)) == (scl | sda);

    /* Khôi phục alternate-function open-drain và I2C1. */
    GPIOB->CRL = (GPIOB->CRL & ~pin_mask) | pin_mask;
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;
    I2C1->CR2 = save_cr2;
    I2C1->CCR = save_crr;
    I2C1->TRISE = save_trise;
    I2C1->CR1 = I2C_CR1_PE;

    return lines_high && ((I2C1->SR2 & I2C_SR2_BUSY) == 0U);
}

static void i2c1_finish_ok_from_isr(void)
{
    BaseType_t task_woken = pdFALSE;

    transaction.status = I2C1_OK;
    transaction.state = STATE_DONE;
    if (transaction.waiting_task != NULL)
    {
        xTaskNotifyFromISR(transaction.waiting_task, 0U, eIncrement, &task_woken);
    }
    portYIELD_FROM_ISR(task_woken);
}

void i2c1_on_event_irq(void)
{
    uint32_t sr1 = I2C1->SR1;

    if (transaction.state == STATE_IDLE || transaction.state == STATE_DONE ||
        transaction.state == STATE_ERROR)
    {
        return;
    }

    if ((sr1 & I2C_SR1_SB) != 0U)
    {
        /* EV5: send the 7-bit address and the read/write bit. */
        I2C1->DR =
            (uint8_t)((transaction.addr7 << 1U) | (transaction.is_read ? 1U : 0U));
        transaction.state = STATE_ADDR;
        return;
    }

    if ((sr1 & I2C_SR1_ADDR) != 0U)
    {
        if (transaction.is_read)
        {
            if (transaction.length == 1U)
            {
                /* EV6_3: NACK the only byte, then request STOP. */
                uint32_t primask = __get_PRIMASK();
                __disable_irq();
                I2C1->CR1 &= ~I2C_CR1_ACK;
                (void)I2C1->SR2;
                I2C1->CR1 |= I2C_CR1_STOP;
                __set_PRIMASK(primask);
            }
            else if (transaction.length == 2U)
            {
                /* EV6_1: POS was set before START; clear ACK after ADDR. */
                uint32_t primask = __get_PRIMASK();
                __disable_irq();
                (void)I2C1->SR2;
                I2C1->CR1 &= ~I2C_CR1_ACK;
                __set_PRIMASK(primask);
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
            else
            {
                (void)I2C1->SR2;
                /* BTF, rather than RXNE, controls multi-byte receive. */
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            }
            transaction.state = STATE_RX;
            return;
        }

        /* EV6 for master transmitter: load the first data byte. */
        (void)I2C1->SR2;
        I2C1->DR = transaction.tx_buffer[transaction.index++];
        if (transaction.index == transaction.length)
        {
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            transaction.state = STATE_WAIT_BTF;
        }
        else
        {
            transaction.state = STATE_TX;
        }
        return;
    }

    if (transaction.is_read && transaction.state == STATE_RX)
    {
        if (transaction.length == 1U && (sr1 & I2C_SR1_RXNE) != 0U)
        {
            transaction.rx_buffer[transaction.index++] = (uint8_t)I2C1->DR;
            i2c1_finish_ok_from_isr();
            return;
        }

        if ((sr1 & I2C_SR1_BTF) != 0U)
        {
            uint16_t remaining = (uint16_t)(transaction.length - transaction.index);

            if (remaining > 3U)
            {
                transaction.rx_buffer[transaction.index++] = (uint8_t)I2C1->DR;
            }
            else if (remaining == 3U)
            {
                /* EV7_2: disable ACK before reading byte N-2. */
                I2C1->CR1 &= ~I2C_CR1_ACK;
                transaction.rx_buffer[transaction.index++] = (uint8_t)I2C1->DR;
            }
            else if (remaining == 2U)
            {
                /* The last two bytes are ready; STOP precedes both reads. */
                uint32_t primask = __get_PRIMASK();
                __disable_irq();
                I2C1->CR1 |= I2C_CR1_STOP;
                transaction.rx_buffer[transaction.index++] = (uint8_t)I2C1->DR;
                transaction.rx_buffer[transaction.index++] = (uint8_t)I2C1->DR;
                __set_PRIMASK(primask);
                i2c1_finish_ok_from_isr();
            }
            return;
        }
        return;
    }

    if (transaction.state == STATE_WAIT_BTF && (sr1 & I2C_SR1_BTF) != 0U)
    {
        /* EV8_2: all transmitted data is on the bus. */
        I2C1->CR1 |= I2C_CR1_STOP;
        i2c1_finish_ok_from_isr();
        return;
    }

    if (transaction.state == STATE_TX && (sr1 & (I2C_SR1_TXE | I2C_SR1_BTF)) != 0U)
    {
        /* EV8: BTF may be set at the same time as TXE. */
        I2C1->DR = transaction.tx_buffer[transaction.index++];
        if (transaction.index == transaction.length)
        {
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
            transaction.state = STATE_WAIT_BTF;
        }
    }
}

void i2c1_on_error_irq(void)
{
    const uint32_t error_mask = I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR |
        I2C_SR1_PECERR | I2C_SR1_TIMEOUT | I2C_SR1_SMBALERT;
    uint32_t errors = I2C1->SR1 & error_mask;
    BaseType_t task_woken = pdFALSE;

    if (errors == 0U)
    {
        return;
    }

    I2C1->SR1 &= ~errors;
    if (transaction.state == STATE_IDLE || transaction.state == STATE_DONE ||
        transaction.state == STATE_ERROR)
    {
        return;
    }

    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);

    if ((errors & I2C_SR1_ARLO) == 0U && (I2C1->SR2 & I2C_SR2_MSL) != 0U)
    {
        I2C1->CR1 |= I2C_CR1_STOP;
    }

    if ((errors &
            (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR | I2C_SR1_PECERR |
                I2C_SR1_SMBALERT)) != 0U)
    {
        transaction.status = I2C1_ERR_BUS;
    }
    else if ((errors & I2C_SR1_TIMEOUT) != 0U)
    {
        transaction.status = I2C1_ERR_TIMEOUT;
    }
    else
    {
        transaction.status = I2C1_ERR_NACK;
    }
    transaction.error_flags = errors;
    transaction.state = STATE_ERROR;

    if (transaction.waiting_task != NULL)
    {
        xTaskNotifyFromISR(transaction.waiting_task, 0U, eIncrement, &task_woken);
    }
    portYIELD_FROM_ISR(task_woken);
}

void I2C1_EV_IRQHandler(void)
{
    i2c1_on_event_irq();
}

void I2C1_ER_IRQHandler(void)
{
    i2c1_on_error_irq();
}
