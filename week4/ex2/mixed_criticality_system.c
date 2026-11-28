#include "mixed_criticality_system.h"
#include "params.h"
#include "system_internal.h"
#include "i2c1_driver.h"
#include "tickless_rtc.h"
#include "queue.h"
#include "stm32f1xx.h"
#include <string.h>

enum
{
    AUX,
    SHELL,
    TELEMETRY,
    IMU,
    CONTROL
};

typedef struct
{
    uint16_t length;
    char text[256];
} reply_t;

typedef struct
{
    char *data;
    uint32_t size;
    uint32_t length;
    bool full;
} text_t;

typedef struct
{
    uint32_t time;
    uint32_t skipped;
} control_stamp_t;

static StaticTask_t task_tcb[W4E2_NUM_TASKS];
static StaticTask_t idle_tcb;
static StackType_t control_stack[256];
static StackType_t imu_stack[384];
static StackType_t telemetry_stack[384];
static StackType_t shell_stack[384];
static StackType_t aux_stack[192];
static StackType_t idle_stack[128];
static TaskHandle_t handles[W4E2_NUM_TASKS];
static const uint32_t stack_words[W4E2_NUM_TASKS] = {192, 384, 384, 384, 256};
static const char *const task_names[] = {"idle_aux",
    "shell",
    "telemetry",
    "imu",
    "control"};
static StaticQueue_t imu_queue_ctrl;
static StaticQueue_t reply_queue_ctrl;
static uint8_t imu_storage[sizeof(w4e2_imu_sample_t)];
static uint8_t reply_storage[2U * sizeof(reply_t)];
static QueueHandle_t imu_queue;
static QueueHandle_t reply_queue;
static w4e2_system_status_t system_status;
static w4e2_imu_sample_t imu_sample;
static volatile bool paused;
static volatile bool resume_pending;
static volatile bool imu_valid;
static volatile int32_t control_value;
static volatile uint32_t timer_high;
static volatile uint32_t telemetry_us = 38462U;
static volatile uint32_t telemetry_due;
static volatile uint32_t control_due;
static volatile uint32_t next_control_due;
static control_stamp_t control_stamps[16];
static volatile uint32_t control_written;
static uint32_t control_read;
static volatile uint32_t missed_ticks;
static volatile uint32_t jitter_max_us;
static volatile uint32_t i2c_errors;
static volatile uint32_t recovery_count;
static volatile uint32_t control_jobs;
static volatile uint32_t imu_jobs;
static uint32_t reply_dropped;

volatile uint32_t wcrt_max[W4E2_NUM_TASKS] W4_MEASURE_STORAGE;
volatile uint32_t stack_min[W4E2_NUM_TASKS];
volatile uint32_t job_count[W4E2_NUM_TASKS] W4_MEASURE_STORAGE;
volatile uint32_t invalid_jobs[W4E2_NUM_TASKS] W4_MEASURE_STORAGE;
volatile uint32_t deadline_misses[W4E2_NUM_TASKS] W4_MEASURE_STORAGE;
volatile uint32_t measured_cpu_x100;
volatile bool measuring;
volatile bool fault_measuring;
volatile bool hang_aux;
static volatile uint32_t release_stamp[W4E2_NUM_TASKS];
static volatile bool release_valid[W4E2_NUM_TASKS];
static volatile uint32_t telemetry_release;
static volatile bool telemetry_pending;
static volatile uint32_t aux_release;
static volatile bool aux_pending;
static uint32_t measure_start_us;
static TickType_t measure_start_tick;
static TickType_t measure_end_tick;
static uint32_t log_dropped;
static uint32_t min_stack_all = UINT32_MAX;
static bool cpu_valid;
static bool cpu_primed;
static volatile bool sampling;
static configRUN_TIME_COUNTER_TYPE previous_total;
static configRUN_TIME_COUNTER_TYPE previous_idle;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *depth)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = 128U;
}

void runtime_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    TIM4->CR1 = 0U;
    TIM4->PSC = 71U;
    TIM4->ARR = 65535U;
    TIM4->CNT = 0U;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR = 0U;
    timer_high = 0U;
    TIM4->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM4_IRQn, 6U);
    NVIC_ClearPendingIRQ(TIM4_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);
    TIM4->CR1 = TIM_CR1_CEN;
}

uint32_t runtime_count(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t high = timer_high;
    uint32_t count = TIM4->CNT;

    if ((TIM4->SR & TIM_SR_UIF) != 0U)
    {
        high += 65536U;
        count = TIM4->CNT;
    }
    __set_PRIMASK(mask);

    return high + count;
}

/* The caller holds a critical section while mapping ticks to the 1 MHz clock. */
static uint32_t tick_stamp(uint32_t expected, uint32_t current)
{
    uint32_t first;
    uint32_t second;
    uint32_t pending;
    uint32_t after;
    uint32_t now;

    do
    {
        pending = SCB->ICSR & SCB_ICSR_PENDSTSET_Msk;
        first = SysTick->VAL;
        now = runtime_count();
        second = SysTick->VAL;
        after = SCB->ICSR & SCB_ICSR_PENDSTSET_Msk;
    } while (second > first || pending != after);

    if (pending != 0U)
    {
        ++current;
    }
    uint32_t phase = (SysTick->LOAD + 1U - first) /
        (configCPU_CLOCK_HZ / 1000000UL);
    return now - phase - (current - expected) * (1000000UL / configTICK_RATE_HZ);
}

void measure_wait(void *task, uint32_t wake, uint32_t current, uint32_t periodic)
{
    if (!measuring && !fault_measuring)
    {
        return;
    }
    uint32_t role;
    if (periodic != 0U && task == handles[IMU])
    {
        role = IMU;
    }
    else if (periodic == 0U && task == handles[AUX])
    {
        role = AUX;
    }
    else
    {
        return;
    }
    release_stamp[role] = tick_stamp(wake, current);
    release_valid[role] = true;
}

static bool record_job(uint32_t role, uint32_t released, uint32_t finished)
{
    if ((!measuring && !fault_measuring) ||
        (int32_t)(released - measure_start_us) < 0)
    {
        return false;
    }
    static const uint32_t deadlines[5] = {100000U, 50000U, 38462U, 2000U, 1000U};
    uint32_t response = finished - released;
    taskENTER_CRITICAL();
    if (response > wcrt_max[role])
    {
        wcrt_max[role] = response;
    }
    ++job_count[role];
    if (response >= deadlines[role])
    {
        ++deadline_misses[role];
    }
    taskEXIT_CRITICAL();
    return true;
}

void i2c_timer_start(uint32_t timeout_us)
{
    TIM4->DIER &= ~TIM_DIER_CC1IE;
    TIM4->CCR1 = (uint16_t)(TIM4->CNT + timeout_us);
    TIM4->SR = (uint16_t)~TIM_SR_CC1IF;
    TIM4->DIER |= TIM_DIER_CC1IE;
}

void i2c_timer_stop(void)
{
    TIM4->DIER &= ~TIM_DIER_CC1IE;
    TIM4->SR = (uint16_t)~TIM_SR_CC1IF;
}

static void telemetry_timer(void)
{
    TIM4->DIER &= ~TIM_DIER_CC2IE;
    telemetry_pending = false;
    if (!paused && telemetry_us != 0U)
    {
        telemetry_due = runtime_count() + telemetry_us;
        TIM4->CCR2 = (uint16_t)telemetry_due;
        TIM4->SR = (uint16_t)~TIM_SR_CC2IF;
        TIM4->DIER |= TIM_DIER_CC2IE;
    }
}

void TIM4_IRQHandler(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t flags = TIM4->SR & TIM4->DIER;

    if ((flags & TIM_SR_UIF) != 0U)
    {
        timer_high += 65536U;
    }
    TIM4->SR = (uint16_t)~flags;
    __set_PRIMASK(mask);
    if ((flags & TIM_SR_CC1IF) != 0U)
    {
        TIM4->DIER &= ~TIM_DIER_CC1IE;
        i2c_timeout_irq();
    }
    if ((flags & TIM_SR_CC2IF) != 0U && !paused && telemetry_us != 0U)
    {
        uint32_t now = runtime_count();
        if ((int32_t)(now - telemetry_due) >= 0)
        {
            uint32_t period = telemetry_us;
            uint32_t skipped = (now - telemetry_due) / period;
            if (!telemetry_pending)
            {
                telemetry_release = telemetry_due;
                telemetry_pending = true;
            }
            else if (measuring || fault_measuring)
            {
                ++invalid_jobs[TELEMETRY];
            }
            if (measuring || fault_measuring)
            {
                invalid_jobs[TELEMETRY] += skipped;
            }
            telemetry_due += (skipped + 1U) * period;
            TIM4->CCR2 = (uint16_t)telemetry_due;
            BaseType_t woken = pdFALSE;
            (void)xTaskNotifyIndexedFromISR(handles[TELEMETRY],
                1U,
                TELEMETRY_DUE,
                eSetBits,
                &woken);
            portYIELD_FROM_ISR(woken);
        }
    }
}

static void control_timer_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << 24U)) | (0xBUL << 24U);
    TIM3->CR1 = 0U;
    TIM3->DIER = 0U;
    /* PA6 doi muc bang phan cung tai moi compare. */
    TIM3->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1;
    TIM3->CCER = 0U;
    TIM3->PSC = 71U;
    TIM3->ARR = 999U;
    TIM3->CCR1 = 999U;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CCER = TIM_CCER_CC1E;
    NVIC_SetPriority(TIM3_IRQn, 5U);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->DIER = TIM_DIER_CC1IE;
}

static void set_paused(bool value)
{
    taskENTER_CRITICAL();

    if (paused == value && (value || (TIM3->CR1 & TIM_CR1_CEN) != 0U))
    {
        taskEXIT_CRITICAL();
        return;
    }
    paused = value;
    TIM3->CR1 = 0U;
    control_written = 0U;
    control_read = 0U;
    TIM3->CNT = 0U;
    TIM3->SR = 0U;
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    control_value = 0;
    imu_valid = false;
    if (!value)
    {
        next_control_due = runtime_count() + 999U;
        TIM3->CR1 = TIM_CR1_CEN;
    }
    telemetry_timer();

    taskEXIT_CRITICAL();
}

void TIM3_IRQHandler(void)
{
    if ((TIM3->SR & TIM_SR_CC1IF) != 0U)
    {
        TIM3->SR = (uint16_t)~TIM_SR_CC1IF;
        uint32_t now = runtime_count();
        uint32_t skipped = (uint32_t)(now - next_control_due) / 1000U;
        if ((int32_t)(now - next_control_due) < 0)
        {
            skipped = 0U;
        }
        control_stamps[control_written % 16U].time = next_control_due;
        control_stamps[control_written % 16U].skipped = skipped;
        __DMB();
        ++control_written;
        missed_ticks += skipped;
        control_due = next_control_due + skipped * 1000U;
        next_control_due = control_due + 1000U;
        mixed_criticality_system_on_control_tick();
    }
}

void mixed_criticality_system_on_control_tick(void)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(handles[CONTROL], &woken);
    portYIELD_FROM_ISR(woken);
}

static int32_t control_step(float acceleration)
{
    int32_t measured = (int32_t)(acceleration * 16384.0f);
    int32_t output = (int32_t)((-(int64_t)measured * 500) / 65536);

    if (output > 1000)
    {
        output = 1000;
    }
    if (output < -1000)
    {
        output = -1000;
    }

    return output;
}

static void control_task(void *parameter)
{
    (void)parameter;
    set_paused(false);
    uint32_t last_sample_tick = 0U;
    bool had_sample = false;

    for (;;)
    {
        uint32_t pending = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (paused)
        {
            continue;
        }
        taskENTER_CRITICAL();
        uint32_t available = control_written - control_read;
        bool timed = pending != 0U && pending <= available && available <= 16U;
        uint32_t released =
            timed ? control_stamps[control_read % 16U].time : control_due;
        uint32_t due = pending;
        if (timed)
        {
            for (uint32_t index = 0U; index < pending; ++index)
            {
                due += control_stamps[(control_read + index) % 16U].skipped;
            }
        }
        control_read = pending <= available ? control_read + pending : control_written;
        taskEXIT_CRITICAL();

        GPIOA->BSRR = GPIO_BSRR_BS2;
        uint32_t start = runtime_count();
        uint32_t late = start - released;
        if (late > jitter_max_us)
        {
            jitter_max_us = late;
        }
        if (pending > 1U)
        {
            system_status.control_overrun_count += pending - 1U;
        }
        taskENTER_CRITICAL();

        uint32_t skipped = missed_ticks;
        system_status.control_overrun_count += missed_ticks;
        missed_ticks = 0U;

        taskEXIT_CRITICAL();
        w4e2_imu_sample_t sample;
        bool received = xQueuePeek(imu_queue, &sample, 0U) == pdPASS;
        if (!received || !had_sample || sample.sample_tick == last_sample_tick)
        {
            ++system_status.imu_stale_count;
        }
        if (received)
        {
            last_sample_tick = sample.sample_tick;
            had_sample = true;
        }
        TickType_t now = xTaskGetTickCount();
        if (!received || !imu_valid ||
            (TickType_t)(now - (TickType_t)sample.sample_tick) > pdMS_TO_TICKS(5U))
        {
            control_value = 0;
        }
        else
        {
            control_value = control_step(sample.accel_g[0]);
        }
        if (runtime_count() - start >= 1000U)
        {
            ++system_status.control_overrun_count;
        }
        GPIOA->BSRR = GPIO_BSRR_BR2;

        uint32_t response = runtime_count() - released;

        if (timed && record_job(CONTROL, released, released + response))
        {
            uint32_t missed = response / 1000U;
            if (missed > due)
            {
                missed = due;
            }
            /* record_job already counted the oldest late job once. */
            if (missed > 1U)
            {
                deadline_misses[CONTROL] += missed - 1U;
            }
            invalid_jobs[CONTROL] += due - 1U;
        }
        else if ((measuring || fault_measuring) && !timed)
        {
            ++invalid_jobs[CONTROL];
            deadline_misses[CONTROL] += pending > 0U ? pending - 1U : 0U;
        }

        ++control_jobs;
        fault_control_done(response, pending > 1U || skipped != 0U || !timed);
    }
}

static i2c1_status_t imu_write(uint8_t reg, uint8_t value)
{
    uint8_t data[] = {reg, value};

    if (!i2c_job_start())
    {
        return I2C1_ERR_BUS;
    }
    i2c1_status_t result = i2c1_master_write(0x68U, data, 2U);
    if (!i2c_job_end())
    {
        result = I2C1_ERR_TIMEOUT;
    }

    return result;
}

static bool imu_setup(void)
{
    if (imu_write(0x6BU, 0x00U) != I2C1_OK)
    {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100U));

    return imu_write(0x1CU, 0x00U) == I2C1_OK && imu_write(0x1AU, 0x03U) == I2C1_OK &&
        imu_write(0x19U, 0x01U) == I2C1_OK;
}

static void imu_task(void *parameter)
{
    (void)parameter;
    bool ready = false;
    bool recovery_sent = false;
    TickType_t release = xTaskGetTickCount();
    TickType_t last_setup = release - pdMS_TO_TICKS(100U);

    for (;;)
    {
        if (paused)
        {
            release_valid[IMU] = false;
            vTaskDelay(pdMS_TO_TICKS(100U));
            release = xTaskGetTickCount();
            continue;
        }
        TickType_t now = xTaskGetTickCount();
        if (!ready && !i2c_fault_pending() && now - last_setup >= pdMS_TO_TICKS(100U))
        {
            last_setup = now;
            release_valid[IMU] = false;
            ready = imu_setup();
            release = xTaskGetTickCount();
        }
        uint32_t released = release_stamp[IMU];
        bool timed = release_valid[IMU];
        release_valid[IMU] = false;
        if (ready && i2c_job_start())
        {
            uint8_t reg = 0x3BU;
            uint8_t raw[6];
            i2c1_status_t result = i2c1_master_write(0x68U, &reg, 1U);
            if (result == I2C1_OK)
            {
                result = i2c1_master_read(0x68U, raw, 6U);
            }
            if (!i2c_job_end())
            {
                result = I2C1_ERR_TIMEOUT;
            }
            if (result == I2C1_OK)
            {
                w4e2_imu_sample_t sample = {0};
                for (uint32_t axis = 0U; axis < 3U; ++axis)
                {
                    int16_t value = (int16_t)(((uint16_t)raw[axis * 2U] << 8U) |
                        raw[axis * 2U + 1U]);
                    sample.accel_g[axis] = (float)value / 16384.0f;
                }
                sample.sample_tick = xTaskGetTickCount();
                taskENTER_CRITICAL();

                imu_sample = sample;
                imu_valid = true;

                taskEXIT_CRITICAL();
                BaseType_t sent = xQueueOverwrite(imu_queue, &sample);
                configASSERT(sent == pdPASS);
            }
            else
            {
                ++i2c_errors;
                imu_valid = false;
            }
        }
        if (i2c_fault_pending())
        {
            imu_valid = false;
            if (!recovery_sent)
            {
                taskENTER_CRITICAL();
                if (!aux_pending)
                {
                    aux_release = runtime_count();
                    aux_pending = true;
                }
                taskEXIT_CRITICAL();
                (void)xTaskNotifyGive(handles[AUX]);
            }
            recovery_sent = true;
        }
        else
        {
            recovery_sent = false;
        }
        if (timed)
        {
            record_job(IMU, released, runtime_count());
        }
        ++imu_jobs;

        if (xTaskDelayUntil(&release, pdMS_TO_TICKS(2U)) == pdFALSE)
        {
            release = xTaskGetTickCount();
            (void)xTaskDelayUntil(&release, pdMS_TO_TICKS(2U));
        }
    }
}

static void append_text(text_t *out, const char *text)
{
    while (*text != '\0')
    {
        if (out->length + 1U < out->size)
        {
            out->data[out->length++] = *text;
        }
        else
        {
            out->full = true;
        }
        ++text;
    }
    if (out->size != 0U)
    {
        out->data[out->length] = '\0';
    }
}

static void append_number(text_t *out, uint32_t value)
{
    char digits[11];
    uint32_t count = 0U;
    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    digits[count] = '\0';
    for (uint32_t a = 0U, b = count - 1U; a < b; ++a, --b)
    {
        char temp = digits[a];
        digits[a] = digits[b];
        digits[b] = temp;
    }
    append_text(out, digits);
}

static void field(text_t *out, const char *name, uint32_t value)
{
    append_text(out, name);
    append_number(out, value);
}

bool system_log(const char *text)
{
    reply_t reply;
    uint32_t length = (uint32_t)strlen(text);
    if (length >= sizeof(reply.text))
    {
        ++log_dropped;
        return false;
    }
    memcpy(reply.text, text, length + 1U);
    reply.length = (uint16_t)length;
    if (xQueueSend(reply_queue, &reply, pdMS_TO_TICKS(20U)) != pdPASS)
    {
        ++log_dropped;
        return false;
    }
    (void)xTaskNotifyIndexed(handles[TELEMETRY], 1U, SHELL_REPLY_READY, eSetBits);
    return true;
}

static bool sample_cpu(void)
{
    taskENTER_CRITICAL();
    configRUN_TIME_COUNTER_TYPE idle = ulTaskGetIdleRunTimeCounter();
    configRUN_TIME_COUNTER_TYPE total = runtime_count();
    taskEXIT_CRITICAL();

    configRUN_TIME_COUNTER_TYPE elapsed = total - previous_total;
    configRUN_TIME_COUNTER_TYPE idle_elapsed = idle - previous_idle;
    cpu_valid = cpu_primed && elapsed != 0U && idle_elapsed <= elapsed;
    if (cpu_valid)
    {
        measured_cpu_x100 =
            (uint32_t)((uint64_t)(elapsed - idle_elapsed) * 10000U / elapsed);
    }
    previous_total = total;
    previous_idle = idle;
    cpu_primed = true;
    return cpu_valid;
}

static bool start_measurement(bool fault)
{
    if (measuring || fault_measuring || sampling || paused ||
        (!fault && telemetry_us == 0U))
    {
        return false;
    }
    cpu_primed = false;
    (void)sample_cpu();
    uart_measure_reset();
    taskENTER_CRITICAL();
    for (uint32_t role = 0U; role < W4E2_NUM_TASKS; ++role)
    {
        wcrt_max[role] = 0U;
        stack_min[role] = 0U;
        job_count[role] = 0U;
        invalid_jobs[role] = 0U;
        deadline_misses[role] = 0U;
    }
    measure_start_tick = xTaskGetTickCount();
    measure_start_us = runtime_count();
    log_dropped = 0U;
    measuring = !fault;
    fault_measuring = fault;
    taskEXIT_CRITICAL();
    return true;
}

bool system_measure_begin(void)
{
    return start_measurement(false);
}

void system_measure_end(void)
{
    taskENTER_CRITICAL();
    if (measuring)
    {
        measure_end_tick = xTaskGetTickCount();
        measuring = false;
    }
    taskEXIT_CRITICAL();
}

void system_measure_sample(uint32_t elapsed_ms, uint32_t extra_stack_min)
{
    sampling = true;
    bool cpu_ok = sample_cpu();
    min_stack_all = (uint32_t)uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    if (extra_stack_min < min_stack_all)
    {
        min_stack_all = extra_stack_min;
    }
    for (uint32_t role = 0U; role < W4E2_NUM_TASKS; ++role)
    {
        stack_min[role] = (uint32_t)uxTaskGetStackHighWaterMark(handles[role]);
        if (stack_min[role] < min_stack_all)
        {
            min_stack_all = stack_min[role];
        }
    }
    for (uint32_t role = 0U; role < W4E2_NUM_TASKS; ++role)
    {
        char line[160];
        text_t out = {line, sizeof(line), 0U, false};
        taskENTER_CRITICAL();
        uint32_t count = job_count[role];
        uint32_t response = wcrt_max[role];
        uint32_t misses = deadline_misses[role];
        uint32_t invalid = invalid_jobs[role];
        taskEXIT_CRITICAL();
        append_text(&out, "M,");
        append_number(&out, elapsed_ms);
        append_text(&out, ",");
        append_text(&out, task_names[role]);
        field(&out, ",", count);
        append_text(&out, ",");
        if (count != 0U)
        {
            append_number(&out, response);
        }
        field(&out, ",", stack_min[role]);
        append_text(&out, ",");
        if (cpu_ok)
        {
            append_number(&out, measured_cpu_x100);
        }
        field(&out, ",", misses);
        field(&out, ",", invalid);
        field(&out, ",", log_dropped);
        append_text(&out, "\r\n");
        (void)system_log(line);
    }
    sampling = false;
}

bool system_fault_begin(void)
{
    return start_measurement(true);
}

void system_fault_end(void)
{
    taskENTER_CRITICAL();
    fault_measuring = false;
    taskEXIT_CRITICAL();
}

bool system_fault_stat(uint32_t role, char *reply, uint32_t size)
{
    configASSERT(role < W4E2_NUM_TASKS);
    text_t out = {reply, size, 0U, false};
    taskENTER_CRITICAL();
    uint32_t count = job_count[role];
    uint32_t response = wcrt_max[role];
    uint32_t misses = deadline_misses[role];
    uint32_t invalid = invalid_jobs[role];
    taskEXIT_CRITICAL();
    append_text(&out, "F,");
    append_text(&out, task_names[role]);
    field(&out, ",", count);
    append_text(&out, ",");
    if (count != 0U)
    {
        append_number(&out, response);
    }
    else
    {
        append_text(&out, "NA");
    }
    field(&out, ",", misses);
    field(&out, ",", invalid);
    append_text(&out, "\r\n");
    return !out.full;
}

bool system_measure_status(char *reply, uint32_t size)
{
    text_t out = {reply, size, 0U, false};
    reply[0] = '\0';
    TickType_t end = measuring ? xTaskGetTickCount() : measure_end_tick;
    field(&out, "measuring=", measuring ? 1U : 0U);
    field(&out, " elapsed_ms=", (end - measure_start_tick) * portTICK_PERIOD_MS);
    append_text(&out, " cpu_x100=");
    if (cpu_valid)
    {
        append_number(&out, measured_cpu_x100);
    }
    else
    {
        append_text(&out, "NA");
    }
    append_text(&out, " min_stack_all=");
    if (min_stack_all != UINT32_MAX)
    {
        append_number(&out, min_stack_all);
    }
    else
    {
        append_text(&out, "NA");
    }
    field(&out, " log_dropped=", log_dropped);
    field(&out, " rx_stamp_lost=", uart_stamp_lost());
    field(&out, " shell_jobs=", job_count[SHELL]);
    append_text(&out, "\r\n");
    return !out.full;
}

void mixed_criticality_system_get_status(w4e2_system_status_t *out)
{
    configASSERT(out != NULL);
    taskENTER_CRITICAL();

    *out = system_status;

    taskEXIT_CRITICAL();
}

static uint32_t cpu_load(void)
{
    static uint32_t previous_total;
    static uint32_t previous_idle;

    taskENTER_CRITICAL();

    uint32_t idle = ulTaskGetIdleRunTimeCounter();
    uint32_t total = runtime_count();

    taskEXIT_CRITICAL();
    uint32_t elapsed = total - previous_total;
    uint32_t idle_elapsed = idle - previous_idle;
    previous_total = total;
    previous_idle = idle;
    if (elapsed == 0U || idle_elapsed > elapsed)
    {
        return 0U;
    }

    return (uint32_t)((uint64_t)(elapsed - idle_elapsed) * 10000U / elapsed);
}

bool mixed_criticality_system_shell_exec(const char *cmd, char *reply, uint32_t size)
{
    if (cmd == NULL || reply == NULL || size == 0U)
    {
        return false;
    }
    text_t out = {reply, size, 0U, false};
    reply[0] = '\0';
    uint32_t length = 0U;

    while (length <= 32U && cmd[length] != '\0')
    {
        ++length;
    }
    if (length > 32U)
    {
        append_text(&out, "error: line too long\r\n");
        return false;
    }
    if (fault_shell_exec(cmd, reply, size))
    {
        return true;
    }
    bool recognized = true;

    if (strcmp(cmd, "status") == 0)
    {
        w4e2_system_status_t status;
        mixed_criticality_system_get_status(&status);
        append_text(&out, paused ? "paused" : "active");
        field(&out, " telemetry_ms=", status.telemetry_period_ms);
        field(&out, " overruns=", status.control_overrun_count);
        field(&out, " stale=", status.imu_stale_count);
        field(&out, " i2c_errors=", i2c_errors);
        field(&out, " recoveries=", recovery_count);
        field(&out, " uart_errors=", uart_errors());
        field(&out, " rx_dropped=", uart_rx_dropped());
        field(&out, " reply_dropped=", reply_dropped);
        field(&out, " jitter_max_us=", jitter_max_us);
        field(&out, " cpu_x100=", cpu_load());
    }
    else if (strcmp(cmd, "stack") == 0)
    {
        append_text(&out, "budget/free_min_words");
        for (uint32_t role = 0U; role < W4E2_NUM_TASKS; ++role)
        {
            append_text(&out, " ");
            append_text(&out, task_names[role]);
            field(&out, "=", stack_words[role]);
            field(&out, "/", (uint32_t)uxTaskGetStackHighWaterMark(handles[role]));
        }
    }
    else if (strncmp(cmd, "telemetry ", 10U) == 0)
    {
        uint32_t period = 0U;
        uint32_t index = 10U;
        bool valid = cmd[index] != '\0';
        while (cmd[index] != '\0')
        {
            if (cmd[index] < '0' || cmd[index] > '9' || period > 6000U)
            {
                valid = false;
                break;
            }
            period = period * 10U + (uint32_t)(cmd[index++] - '0');
        }
        if (!valid || period > 60000U || (period != 0U && period < 39U))
        {
            append_text(&out, "error: telemetry 0 or 39..60000 ms");
            recognized = false;
        }
        else
        {
            taskENTER_CRITICAL();

            telemetry_us = period * 1000U;
            system_status.telemetry_period_ms = period;
            telemetry_timer();

            taskEXIT_CRITICAL();
            append_text(&out, "ok");
        }
    }
    else if (strcmp(cmd, "pause") == 0 || strcmp(cmd, "resume") == 0)
    {
        set_paused(strcmp(cmd, "pause") == 0);
        append_text(&out, paused ? "paused; PB0 to GND wakes/resumes" : "active");
    }
    else
    {
        append_text(&out, "commands: status, stack, telemetry N, pause, resume");
        recognized = false;
    }
    append_text(&out, "\r\n");

    return recognized && !out.full;
}

static void shell_task(void *parameter)
{
    (void)parameter;
    char line[33];
    uint32_t length = 0U;
    bool discard = false;
    TickType_t started = 0U;

    for (;;)
    {
        int byte = uart_read_byte(length != 0U || discard ? pdMS_TO_TICKS(10U)
                                                          : portMAX_DELAY);
        if ((length != 0U || discard) &&
            xTaskGetTickCount() - started >= pdMS_TO_TICKS(W4E2_SHELL_CMD_TIMEOUT_MS))
        {
            length = 0U;
            discard = true;
        }
        if (byte < 0)
        {
            continue;
        }
        if (byte == '\r' || byte == '\n')
        {
            if (length != 0U && !discard)
            {
                bool measure_this = measuring || fault_measuring;
                uint32_t released;
                bool timed = uart_line_release(&released);
                line[length] = '\0';
                reply_t reply;
                (void)mixed_criticality_system_shell_exec(
                    line, reply.text, sizeof(reply.text));
                reply.length = (uint16_t)strlen(reply.text);
                if (xQueueSend(reply_queue, &reply, 0U) != pdPASS)
                {
                    ++reply_dropped;
                }
                (void)xTaskNotifyIndexed(
                    handles[TELEMETRY], 1U, SHELL_REPLY_READY, eSetBits);
                if (measure_this && timed)
                {
                    record_job(SHELL, released, runtime_count());
                }
                else if (measure_this && (measuring || fault_measuring))
                {
                    ++invalid_jobs[SHELL];
                }
                vTaskDelay(pdMS_TO_TICKS(50U));
            }
            length = 0U;
            discard = false;
        }
        else if (!discard)
        {
            if (length == 0U)
            {
                started = xTaskGetTickCount();
            }
            if (length < 32U)
            {
                line[length++] = (char)byte;
            }
            else
            {
                discard = true;
            }
        }
    }
}

static void telemetry_task(void *parameter)
{
    (void)parameter;
    static const uint8_t banner[] =
        "# W4E2 2413032; 921600 8N1; commands end with CR/LF\r\n";
    (void)uart_dma_send(banner, sizeof(banner) - 1U);
    taskENTER_CRITICAL();

    telemetry_timer();

    taskEXIT_CRITICAL();
    uint32_t sequence = 0U;

    for (;;)
    {
        uint32_t events = 0U;
        (void)xTaskNotifyWaitIndexed(1U, 0U, UINT32_MAX, &events, portMAX_DELAY);
        uint32_t released = 0U;
        bool timed = false;
        if ((events & TELEMETRY_DUE) != 0U)
        {
            taskENTER_CRITICAL();
            released = telemetry_release;
            timed = telemetry_pending;
            telemetry_pending = false;
            taskEXIT_CRITICAL();
        }
        reply_t reply;
        if (xQueueReceive(reply_queue, &reply, 0U) == pdPASS)
        {
            (void)uart_dma_send((const uint8_t *)reply.text, reply.length);
            if (uxQueueMessagesWaiting(reply_queue) != 0U)
            {
                (void)xTaskNotifyIndexed(handles[TELEMETRY],
                    1U,
                    SHELL_REPLY_READY,
                    eSetBits);
            }
        }
        if ((events & TELEMETRY_DUE) != 0U && !paused && telemetry_us != 0U)
        {
            uint8_t payload[SYSTEM_PAYLOAD_BYTES] = {0};
            uint8_t frame[SYSTEM_FRAME_BYTES];
            w4e2_system_status_t status;
            w4e2_imu_sample_t sample;
            mixed_criticality_system_get_status(&status);
            taskENTER_CRITICAL();

            sample = imu_sample;

            taskEXIT_CRITICAL();
            uint32_t words[] = {sample.sample_tick,
                (uint32_t)control_value,
                status.control_overrun_count,
                status.imu_stale_count,
                i2c_errors,
                uart_errors(),
                jitter_max_us};
            memcpy(payload, &words[0], 4U);
            memcpy(payload + 4U, sample.accel_g, 12U);
            memcpy(payload + 16U, &words[1], 24U);
            payload[40] = imu_valid ? 1U : 0U;
            uart_pack_frame(frame, payload, sequence++);
            bool sent = uart_dma_send(frame, sizeof(frame));
            if (timed)
            {
                record_job(TELEMETRY, released, runtime_count());
            }
            if ((measuring || fault_measuring) && (!sent || !timed))
            {
                ++invalid_jobs[TELEMETRY];
            }
        }
    }
}

bool fault_system_ready(uint32_t *errors)
{
    taskENTER_CRITICAL();
    *errors = i2c_errors;
    bool ready = !paused && imu_valid;
    taskEXIT_CRITICAL();
    return ready;
}

__attribute__((weak))
uint32_t fault_poll(uint32_t control_count, uint32_t imu_count, bool paused,
    bool valid, int32_t output, uint32_t i2c_count, uint32_t overruns)
{
    (void)control_count;
    (void)imu_count;
    (void)paused;
    (void)valid;
    (void)output;
    (void)i2c_count;
    (void)overruns;
    return 100U;
}

__attribute__((weak))
void fault_control_done(uint32_t response_us, bool lost)
{
    (void)response_us;
    (void)lost;
}

__attribute__((weak))
bool fault_shell_exec(const char *cmd, char *reply, uint32_t size)
{
    (void)cmd;
    (void)reply;
    (void)size;
    return false;
}

static void aux_task(void *parameter)
{
    (void)parameter;
    TickType_t last_recovery = xTaskGetTickCount() - pdMS_TO_TICKS(100U);
    uint32_t released = 0U;
    bool timed = false;

    for (;;)
    {
        uint32_t wait_ms = fault_poll(control_jobs, imu_jobs, paused,
            imu_valid, control_value, i2c_errors, system_status.control_overrun_count);
        if (timed)
        {
            record_job(AUX, released, runtime_count());
        }
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
        taskENTER_CRITICAL();
        released = notified != 0U ? aux_release : release_stamp[AUX];
        timed = notified != 0U ? aux_pending : release_valid[AUX];
        aux_pending = false;
        release_valid[AUX] = false;
        taskEXIT_CRITICAL();

        if (hang_aux)
        {
            for (;;)
            {
                __NOP();
            }
        }

        if (resume_pending)
        {
            resume_pending = false;
            set_paused(false);
        }
        TickType_t now = xTaskGetTickCount();
        if (i2c_fault_pending() && now - last_recovery >= pdMS_TO_TICKS(100U))
        {
            last_recovery = now;
            if (i2c_recover())
            {
                ++recovery_count;
            }
        }
    }
}

void EXTI0_IRQHandler(void)
{
    if ((EXTI->PR & EXTI_PR_PR0) != 0U)
    {
        EXTI->PR = EXTI_PR_PR0;
        if (paused && handles[AUX] != NULL)
        {
            resume_pending = true;
            BaseType_t woken = pdFALSE;
            vTaskNotifyGiveFromISR(handles[AUX], &woken);
            portYIELD_FROM_ISR(woken);
        }
    }
}

static void shallow_sleep(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
        __DSB();
        __WFI();
        __ISB();
    }
    __set_PRIMASK(mask);
}

void vApplicationIdleHook(void)
{
    if (!paused)
    {
        shallow_sleep();
    }
}

void sleep_idle(uint32_t expected_ticks)
{
    if (paused && !i2c_busy() && uart_idle())
    {
        TIM4->CR1 &= ~TIM_CR1_CEN;
        tickless_suppress_ticks_and_sleep(expected_ticks);
        TIM4->CR1 |= TIM_CR1_CEN;
    }
    else
    {
        shallow_sleep();
    }
}

__attribute__((weak)) void system_rtc_init(void)
{
    tickless_rtc_init();
}

void mixed_criticality_system_init(void)
{
    _Static_assert(W4E2_NUM_TASKS == 5, "Expected five application tasks");
    system_status.telemetry_period_ms = 39U;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    GPIOA->CRL =
        (GPIOA->CRL & ~((0xFUL << 4U) | (0xFUL << 8U))) | (0x2UL << 4U) | (0x2UL << 8U);
    GPIOA->BSRR = GPIO_BSRR_BR1 | GPIO_BSRR_BR2;
    GPIOB->CRL = (GPIOB->CRL & ~0xFUL) | 0x8UL;
    GPIOB->BSRR = GPIO_BSRR_BS0;
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~0xFUL) | 0x1UL;
    EXTI->FTSR |= EXTI_FTSR_TR0;
    EXTI->RTSR &= ~EXTI_RTSR_TR0;
    EXTI->PR = EXTI_PR_PR0;
    EXTI->IMR |= EXTI_IMR_MR0;
    NVIC_SetPriority(EXTI0_IRQn, 6U);
    NVIC_EnableIRQ(EXTI0_IRQn);
    i2c1_driver_init();
    uart_dma_init();
    system_rtc_init();
    control_timer_init();
    imu_queue =
        xQueueCreateStatic(1U, sizeof(w4e2_imu_sample_t), imu_storage, &imu_queue_ctrl);
    reply_queue =
        xQueueCreateStatic(2U, sizeof(reply_t), reply_storage, &reply_queue_ctrl);
    configASSERT(imu_queue != NULL && reply_queue != NULL);
    StackType_t *stacks[] = {aux_stack,
        shell_stack,
        telemetry_stack,
        imu_stack,
        control_stack};
    TaskFunction_t functions[] = {aux_task,
        shell_task,
        telemetry_task,
        imu_task,
        control_task};
    for (uint32_t role = 0U; role < W4E2_NUM_TASKS; ++role)
    {
        handles[role] = xTaskCreateStatic(functions[role],
            task_names[role],
            stack_words[role],
            NULL,
            W4E2_TASK_PRIORITY[role],
            stacks[role],
            &task_tcb[role]);
        configASSERT(handles[role] != NULL);
    }
}
