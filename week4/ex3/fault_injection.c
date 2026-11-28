#include "fault_injection.h"
#include "include/params.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "stm32f1xx.h"
#include "system_internal.h"
#include "mixed_criticality_system.h"
#include <string.h>
#include "tickless_rtc.h"

extern volatile uint32_t wcrt_max[5];
extern volatile uint32_t job_count[5];
extern volatile uint32_t invalid_jobs[5];
extern volatile uint32_t deadline_misses[5];
extern volatile bool hang_aux;

#define RESET_MAGIC 0x57344533UL
#define FAULT_READY (1UL << 2U)
#define RTC_MAGIC 0x4F33U

typedef struct
{
    uint32_t magic;
    uint32_t reason;
    uint32_t missed;
    uint32_t bytes;
    uint32_t start_rtc;
    uint32_t boot_rtc;
    uint32_t ready_rtc;
    uint32_t rtc_valid;
    uint32_t starved_us;
    uint32_t late_us;
} reset_record_t;

static volatile reset_record_t reset_record __attribute__((section(".noinit")));

static StaticEventGroup_t event_storage;
static EventGroupHandle_t fault_events;
static StaticTask_t runner_tcb;
static StaticTask_t overload_tcb;
static StackType_t runner_stack[256];
static StackType_t overload_stack[192];
static TaskHandle_t runner_handle;
static TaskHandle_t overload_handle;
static bool watchdog_started;
static bool recovered;
static bool recovery_ready;
static bool fault_stats_valid;
static volatile bool ready;
static volatile bool busy;
static volatile bool active;
static volatile bool mode_paused;
static volatile bool have_result;
static uint32_t last_control;
static uint32_t last_imu;
static uint32_t rx_before;
static uint32_t overruns_before;
static uint32_t i2c_before;
static TickType_t watchdog_start;
static volatile TickType_t measure_started;
static volatile TickType_t next_sample;
static volatile bool measure_header;
static volatile bool measure_footer;

static uint32_t rtc_count(void)
{
    uint32_t high;
    uint32_t low;
    do
    {
        high = RTC->CNTH;
        low = RTC->CNTL;
    } while (high != RTC->CNTH);
    return (high << 16U) | low;
}

static bool rtc_clock_ok(void)
{
    const uint32_t mask =
        RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN | RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    return (RCC->BDCR & mask) ==
               (RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN | RCC_BDCR_LSEON | RCC_BDCR_LSERDY) &&
           BKP->DR10 == RTC_MAGIC;
}

void system_rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    if (!rtc_clock_ok())
    {
        tickless_rtc_init();
        BKP->DR10 = RTC_MAGIC;
        return;
    }

    /* Keep the LSE counter running across watchdog resets. */
    RTC->CRL &= ~RTC_CRL_RSF;
    while ((RTC->CRL & RTC_CRL_RSF) == 0U)
    {
    }
    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U)
    {
    }
    RTC->CRL &= ~(RTC_CRL_ALRF | RTC_CRL_SECF | RTC_CRL_OWF);
    RTC->CRH = RTC_CRH_ALRIE;
    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U)
    {
    }
    EXTI->FTSR &= ~EXTI_FTSR_TR17;
    EXTI->RTSR |= EXTI_RTSR_TR17;
    EXTI->PR = EXTI_PR_PR17;
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    NVIC_SetPriority(RTC_Alarm_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    EXTI->IMR |= EXTI_IMR_MR17;
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

static uint32_t rtc_us(uint32_t counts)
{
    return (uint32_t)(((uint64_t)counts * 1000000U + 32767U) / 32768U);
}

static void add_number(char *text, uint32_t value)
{
    char digits[11];
    uint32_t count = 0U;
    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    uint32_t end = (uint32_t)strlen(text);
    while (count != 0U)
    {
        text[end++] = digits[--count];
    }
    text[end] = '\0';
}

static void measure_log(void)
{
    if (measuring && !measure_header)
    {
        (void)system_log(
            "# measure_begin; cpu_api=ulTaskGetIdleRunTimeCounter; static storage\r\n");
        (void)system_log("# vTaskGetRunTimeStats requires dynamic allocation in this "
                         "kernel; mentor confirmation needed\r\n");
        (void)system_log("# shell release=RX terminator available; telemetry includes "
                         "DMA wait; IMU setup excluded\r\n");
        (void)system_log(
            "# aux monitor=20ms; recovery rate=100ms; stack minimum is since boot\r\n");
        (void)system_log("# "
                         "M,time_ms,task,jobs,wcrt_us,stack_free_words,cpu_x100,"
                         "deadline_misses,invalid_jobs,log_dropped\r\n");
        measure_header = true;
    }
    TickType_t now = xTaskGetTickCount();
    if (measuring && (int32_t)(now - next_sample) >= 0)
    {
        uint32_t elapsed = (now - measure_started) * portTICK_PERIOD_MS;
        bool finished = elapsed >= 600000U;
        if (finished)
        {
            system_measure_end();
        }
        uint32_t runner_free = (uint32_t)uxTaskGetStackHighWaterMark(runner_handle);
        uint32_t overload_free = (uint32_t)uxTaskGetStackHighWaterMark(overload_handle);
        system_measure_sample(elapsed,
                              runner_free < overload_free ? runner_free : overload_free);
        next_sample +=
            ((now - next_sample) / pdMS_TO_TICKS(1000U) + 1U) * pdMS_TO_TICKS(1000U);
        if (finished)
        {
            measure_footer = true;
        }
    }
    if (!measuring && measure_footer)
    {
        char status[160];
        (void)system_measure_status(status, sizeof(status));
        (void)system_log(status);
        (void)system_log("# measure_end\r\n");
        measure_footer = false;
    }
}

static w4e3_fault_result_t last_result;
static w4e3_verification_sample_t samples[5] = {
    {.wcrt_predicted_us = 5868U}, /* idle_aux */
    {.wcrt_predicted_us = 1928U}, /* shell */
    {.wcrt_predicted_us = 1778U}, /* telemetry */
    {.wcrt_predicted_us = 1528U}, /* imu */
    {.wcrt_predicted_us = 118U}   /* control */
};

static void overload_task(void *parameter)
{
    (void)parameter;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint32_t start = runtime_count();

    for (;;)
    {
        /* Priority 9 prevents control from completing a job while this runs. */
        uint32_t elapsed = runtime_count() - start;
        if (elapsed >= 2000U)
        {
            reset_record.missed = 1U;
            reset_record.starved_us = elapsed;
            __DSB();
        }
        __NOP();
    }
}

static void fault_task(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        TickType_t wait = portMAX_DELAY;
        if (measuring)
        {
            TickType_t now = xTaskGetTickCount();
            wait = (int32_t)(next_sample - now) > 0 ? next_sample - now : 0U;
        }
        uint32_t kind;
        BaseType_t notified = xTaskNotifyWait(0U, UINT32_MAX, &kind, wait);
        if (notified == pdTRUE && kind < (uint32_t)W4E3_FAULT_COUNT)
        {
            fault_injection_trigger((w4e3_fault_kind_t)kind);
            taskENTER_CRITICAL();
            busy = false;
            taskEXIT_CRITICAL();
        }
        measure_log();
    }
}

void fault_injection_init(uint32_t *out_prev_reason)
{
    configASSERT(out_prev_reason != NULL);
    *out_prev_reason = 0U;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    bool clock_ok = rtc_clock_ok();
    if (clock_ok)
    {
        RTC->CRL &= ~RTC_CRL_RSF;
        while ((RTC->CRL & RTC_CRL_RSF) == 0U)
        {
        }
    }

    if ((RCC->CSR & RCC_CSR_IWDGRSTF) != 0U && reset_record.magic == RESET_MAGIC &&
        reset_record.bytes == sizeof(reset_record) && reset_record.reason > 0U &&
        reset_record.reason <= (uint32_t)W4E3_FAULT_COUNT)
    {
        *out_prev_reason = reset_record.reason;
        last_result.kind = (w4e3_fault_kind_t)(reset_record.reason - 1U);
        last_result.watchdog_reset_reason_code = reset_record.reason;
        last_result.control_deadline_missed = reset_record.missed != 0U;
        last_result.was_declared_controlled_degradation =
            last_result.kind == W4E3_FAULT_UART_FLOOD ||
            last_result.kind == W4E3_FAULT_SENSOR_UNPLUG;
        have_result = true;
        recovered = true;
        fault_stats_valid = true;
        reset_record.rtc_valid = clock_ok && reset_record.rtc_valid != 0U;
        reset_record.boot_rtc = rtc_count();
    }
    else
    {
        reset_record.rtc_valid = 0U;
        reset_record.starved_us = 0U;
        reset_record.late_us = 0U;
        for (uint32_t role = 0U; role < 5U; ++role)
        {
            wcrt_max[role] = 0U;
            job_count[role] = 0U;
            invalid_jobs[role] = 0U;
            deadline_misses[role] = 0U;
        }
    }

    reset_record.magic = 0U;
    RCC->CSR |= RCC_CSR_RMVF;
    fault_events = xEventGroupCreateStatic(&event_storage);
    configASSERT(fault_events != NULL);
    if (*out_prev_reason != 0U)
    {
        (void)xEventGroupSetBits(fault_events, W4E3_BIT_WATCHDOG_RECOVERED);
    }

    runner_handle =
        xTaskCreateStatic(fault_task, "fault", 256U, NULL, 4U, runner_stack, &runner_tcb);
    overload_handle = xTaskCreateStatic(overload_task, "overload", 192U, NULL, 9U,
                                        overload_stack, &overload_tcb);
    configASSERT(runner_handle != NULL && overload_handle != NULL);

    RCC->CSR |= RCC_CSR_LSION;
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U)
    {
    }

    IWDG->KR = 0x5555U;
    IWDG->PR = 3U;
    IWDG->RLR = 124U;
    while (IWDG->SR != 0U)
    {
    }
    IWDG->KR = 0xAAAAU;
}

void fault_watchdog_start(void)
{
    watchdog_start = xTaskGetTickCount();
    watchdog_started = true;
    IWDG->KR = 0xAAAAU;
    IWDG->KR = 0xCCCCU;
}

void fault_control_done(uint32_t response_us, bool lost)
{
    if (!fault_measuring || (response_us < 1000U && !lost))
    {
        return;
    }

    taskENTER_CRITICAL();
    last_result.control_deadline_missed = true;
    if (response_us >= 1000U && response_us - 1000U > reset_record.late_us)
    {
        reset_record.late_us = response_us - 1000U;
    }
    reset_record.missed = 1U;
    __DSB();
    taskEXIT_CRITICAL();
}

uint32_t fault_poll(uint32_t control_count, uint32_t imu_count, bool paused, bool valid,
                    int32_t output, uint32_t i2c_count, uint32_t overruns)
{
    bool control_live = control_count != last_control;
    bool imu_live = imu_count != last_imu;
    bool healthy = paused || (control_live && imu_live);
    TickType_t now = xTaskGetTickCount();
    mode_paused = paused;

    if (!ready)
    {
        /* IMU initialization deliberately waits 100 ms. */
        healthy =
            control_live && (imu_live || now - watchdog_start < pdMS_TO_TICKS(200U));
        if (control_live && imu_live && valid && !paused)
        {
            ready = true;
            if (recovered && reset_record.rtc_valid != 0U)
            {
                reset_record.ready_rtc = rtc_count();
                recovery_ready = true;
            }
            (void)xEventGroupSetBits(fault_events, FAULT_READY);
        }
    }
    last_control = control_count;
    last_imu = imu_count;

    if (watchdog_started && healthy)
    {
        IWDG->KR = 0xAAAAU;
    }

    bool degraded = false;
    taskENTER_CRITICAL();
    if (fault_measuring && control_live && !paused)
    {
        if (overruns != overruns_before)
        {
            last_result.control_deadline_missed = true;
            reset_record.missed = 1U;
        }
        if (last_result.kind == W4E3_FAULT_UART_FLOOD)
        {
            degraded = uart_rx_dropped() != rx_before;
        }
        else if (last_result.kind == W4E3_FAULT_SENSOR_UNPLUG)
        {
            degraded = i2c_count != i2c_before && !valid && output == 0;
        }
    }
    taskEXIT_CRITICAL();

    if (degraded)
    {
        (void)xEventGroupSetBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED);
    }
    return 20U;
}

void fault_injection_trigger(w4e3_fault_kind_t kind)
{
    configASSERT((uint32_t)kind < (uint32_t)W4E3_FAULT_COUNT);
    configASSERT(fault_events != NULL);
    configASSERT(watchdog_started);
    configASSERT(!measuring && !fault_measuring);
    (void)xEventGroupWaitBits(fault_events, FAULT_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    (void)xEventGroupClearBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED);

    bool started = system_fault_begin();
    configASSERT(started);
    w4e2_system_status_t status;
    taskENTER_CRITICAL();
    configASSERT(!active);
    configASSERT(fault_system_ready(&i2c_before));
    mixed_criticality_system_get_status(&status);
    last_result.kind = kind;
    last_result.control_deadline_missed = false;
    last_result.was_declared_controlled_degradation =
        kind == W4E3_FAULT_UART_FLOOD || kind == W4E3_FAULT_SENSOR_UNPLUG;
    last_result.watchdog_reset_reason_code = 0U;
    have_result = true;
    overruns_before = status.control_overrun_count;
    rx_before = uart_rx_dropped();
    reset_record.magic = 0U;
    reset_record.reason = (uint32_t)kind + 1U;
    reset_record.missed = 0U;
    reset_record.bytes = sizeof(reset_record);
    reset_record.rtc_valid = rtc_clock_ok() ? 1U : 0U;
    reset_record.start_rtc = rtc_count();
    reset_record.boot_rtc = 0U;
    reset_record.ready_rtc = 0U;
    reset_record.starved_us = 0U;
    reset_record.late_us = 0U;
    recovered = false;
    fault_stats_valid = true;
    recovery_ready = false;
    __DSB();
    reset_record.magic = RESET_MAGIC;
    __DSB();
    active = true;

    if (kind == W4E3_FAULT_TASK_HANG)
    {
        hang_aux = true;
    }
    taskEXIT_CRITICAL();

    if (kind == W4E3_FAULT_CPU_OVERLOAD)
    {
        (void)xTaskNotifyGive(overload_handle);
    }

    /* UART flood comes from the host; sensor unplug is a physical action. */
    (void)xEventGroupWaitBits(fault_events, W4E3_BIT_DEGRADED_MODE_ENTERED, pdTRUE,
                              pdTRUE, portMAX_DELAY);

    taskENTER_CRITICAL();
    active = false;
    taskEXIT_CRITICAL();
}

bool fault_shell_exec(const char *cmd, char *reply, uint32_t size)
{
    static const char *const commands[] = {"fault hang", "fault uart", "fault sensor",
                                           "fault cpu"};
    char message[128];
    configASSERT(size > 0U);

    if (strcmp(cmd, "measure status") == 0)
    {
        return system_measure_status(reply, size);
    }
    if (strcmp(cmd, "measure start") == 0)
    {
        uint32_t errors;
        if (!ready || busy || measuring || fault_measuring || measure_footer ||
            !fault_system_ready(&errors) || !system_measure_begin())
        {
            strcpy(message,
                   "error: wait for startup; keep telemetry on and finish current test");
        }
        else
        {
            fault_stats_valid = false;
            measure_started = xTaskGetTickCount();
            next_sample = measure_started + pdMS_TO_TICKS(1000U);
            measure_header = false;
            (void)xTaskNotify(runner_handle, UINT32_MAX, eSetValueWithoutOverwrite);
            strcpy(message,
                   "measure started; keep telemetry on; no faults for 600 seconds");
        }
    }
    else if (strcmp(cmd, "measure stop") == 0)
    {
        if (measuring)
        {
            system_measure_end();
            measure_footer = true;
            (void)xTaskNotify(runner_handle, UINT32_MAX, eSetValueWithoutOverwrite);
        }
        strcpy(message, "measure stopped");
    }
    else if ((measuring || fault_measuring) &&
             (strcmp(cmd, "pause") == 0 || strncmp(cmd, "telemetry ", 10U) == 0))
    {
        strcpy(message, "error: stop measurement before changing the workload");
    }
    else if (strcmp(cmd, "pause") == 0 && busy)
    {
        strcpy(message, "error: finish current fault before pause");
    }
    else if (strcmp(cmd, "fault stop") == 0)
    {
        if (busy)
        {
            strcpy(message, "error: wait for degraded-mode observation");
        }
        else
        {
            system_fault_end();
            reset_record.magic = 0U;
            strcpy(message, "fault capture stopped");
        }
    }
    else if (strncmp(cmd, "fault stat ", 11U) == 0 && cmd[11] >= '0' && cmd[11] <= '4' &&
             cmd[12] == '\0')
    {
        if (fault_stats_valid)
        {
            return system_fault_stat((uint32_t)(cmd[11] - '0'), reply, size);
        }
        strcpy(message, "no retained fault measurements");
    }
    else if (strcmp(cmd, "fault timing") == 0)
    {
        strcpy(message, "boot_us=");
        if (recovered && reset_record.rtc_valid != 0U)
        {
            add_number(message, rtc_us(reset_record.boot_rtc - reset_record.start_rtc));
        }
        else
        {
            strcat(message, "NA");
        }
        strcat(message, " ready_us=");
        if (recovery_ready)
        {
            add_number(message, rtc_us(reset_record.ready_rtc - reset_record.start_rtc));
        }
        else
        {
            strcat(message, "NA");
        }
        strcat(message, " starved_us=");
        add_number(message, reset_record.starved_us);
        strcat(message, " late_us=");
        add_number(message, reset_record.late_us);
    }
    else if (strcmp(cmd, "fault result") == 0)
    {
        w4e3_fault_result_t result;
        fault_injection_get_last_result(&result);
        if (!have_result)
        {
            strcpy(message, "no fault result");
        }
        else
        {
            strcpy(message, "fault=");
            strcat(message, W4E3_ASSIGNED_FAULT_NAMES[result.kind]);
            strcat(message, busy ? " pending" : " finished");
            strcat(message, result.watchdog_reset_reason_code != 0U ? " reset=IWDG"
                                                                    : " reset=none");
            strcat(message, result.control_deadline_missed ? " missed=yes"
                                                           : " missed=not_observed");
            strcat(message, result.was_declared_controlled_degradation ? " declared=yes"
                                                                       : " declared=no");
            strcat(message, fault_measuring ? " capture=on" : " capture=off");
        }
    }
    else
    {
        uint32_t kind;
        for (kind = 0U; kind < (uint32_t)W4E3_FAULT_COUNT; ++kind)
        {
            if (strcmp(cmd, commands[kind]) == 0)
            {
                break;
            }
        }
        if (kind == (uint32_t)W4E3_FAULT_COUNT)
        {
            return false;
        }
        taskENTER_CRITICAL();
        uint32_t errors;
        bool accepted = ready && !mode_paused && !busy && !measuring &&
                        !fault_measuring && fault_system_ready(&errors);
        if (accepted)
        {
            busy = true;
        }
        taskEXIT_CRITICAL();
        if (!accepted)
        {
            strcpy(message, "error: wait for startup/resume or current fault");
        }
        else
        {
            BaseType_t sent = xTaskNotify(runner_handle, kind, eSetValueWithoutOverwrite);
            configASSERT(sent == pdPASS);
            strcpy(message, "armed ");
            strcat(message, W4E3_ASSIGNED_FAULT_NAMES[kind]);
            if (kind == W4E3_FAULT_UART_FLOOD)
            {
                strcat(message, "; send UART data continuously");
            }
            else if (kind == W4E3_FAULT_SENSOR_UNPLUG)
            {
                strcat(message, "; disconnect sensor SDA");
            }
        }
    }
    uint32_t length = (uint32_t)strlen(message);
    if (length + 3U > size)
    {
        reply[0] = '\0';
        return false;
    }
    memcpy(reply, message, length);
    memcpy(reply + length, "\r\n", 3U);
    return true;
}

void fault_injection_get_verification_sample(uint32_t role_index,
                                             w4e3_verification_sample_t *out)
{
    configASSERT(out != NULL);
    configASSERT(role_index < 5U);
    taskENTER_CRITICAL();
    *out = samples[role_index];
    out->wcrt_measured_us = wcrt_max[role_index];
    out->stack_high_water_mark_words = stack_min[role_index];
    out->cpu_load_percent = measured_cpu_x100 / 100U;
    taskEXIT_CRITICAL();
}

void fault_injection_get_last_result(w4e3_fault_result_t *out)
{
    configASSERT(out != NULL);
    taskENTER_CRITICAL();
    *out = last_result;
    taskEXIT_CRITICAL();
}
