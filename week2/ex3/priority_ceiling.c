#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"
#include "priority_ceiling.h"

/* W2.3 has exactly two protected resources. Only task context calls this
   wrapper; the critical section makes admission, take, and boost atomic. */
static pc_mutex_t *registered_mutexes[2];
static TaskHandle_t owners[2];
static uint32_t registered_count;
typedef struct {
    TaskHandle_t task;
    StaticSemaphore_t storage;
    SemaphoreHandle_t signal;
} pc_waiter_t;

static pc_waiter_t waiters[4];

static uint32_t waiter_index(TaskHandle_t task)
{
    for (uint32_t i = 0U; i < 4U; ++i) {
        if (waiters[i].task == task) {
            return i;
        }
    }
    for (uint32_t i = 0U; i < 4U; ++i) {
        if (waiters[i].task == NULL) {
            waiters[i].task = task;
            /* Discard a signal left by the previous user of this slot. */
            (void)xSemaphoreTake(waiters[i].signal, 0U);
            return i;
        }
    }
    configASSERT(0);
    return 0U;
}

static uint32_t mutex_index(const pc_mutex_t *m)
{
    for (uint32_t i = 0U; i < registered_count; ++i) {
        if (registered_mutexes[i] == m) {
            return i;
        }
    }
    configASSERT(0);
    return 0U;
}

void pc_mutex_create_static(pc_mutex_t *m, UBaseType_t ceiling_prio)
{
    configASSERT(m != NULL);
    configASSERT(ceiling_prio < configMAX_PRIORITIES);
    configASSERT(registered_count < 2U);

    if (registered_count == 0U) {
        for (uint32_t i = 0U; i < 4U; ++i) {
            waiters[i].task = NULL;
            waiters[i].signal = xSemaphoreCreateBinaryStatic(&waiters[i].storage);
            configASSERT(waiters[i].signal != NULL);
        }
    }

    m->handle = xSemaphoreCreateMutexStatic(&m->storage);
    configASSERT(m->handle != NULL);
    m->ceiling_prio = ceiling_prio;
    m->saved_prio = 0U;
    registered_mutexes[registered_count] = m;
    owners[registered_count] = NULL;
    ++registered_count;
}

void pc_mutex_lock(pc_mutex_t *m)
{
    configASSERT(m != NULL);
    configASSERT(m->handle != NULL);
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    uint32_t target = mutex_index(m);

    for (;;) {
        taskENTER_CRITICAL();
        configASSERT(owners[target] != self);

        /* Use the priority from before this task's own ceiling boosts. */
        UBaseType_t base_prio = uxTaskPriorityGet(NULL);
        for (uint32_t i = 0U; i < registered_count; ++i) {
            if (owners[i] == self && registered_mutexes[i]->saved_prio < base_prio) {
                base_prio = registered_mutexes[i]->saved_prio;
            }
        }

        BaseType_t admitted = pdTRUE;
        for (uint32_t i = 0U; i < registered_count; ++i) {
            if (owners[i] != NULL && owners[i] != self &&
                base_prio <= registered_mutexes[i]->ceiling_prio) {
                admitted = pdFALSE;
                break;
            }
        }

        if (admitted == pdTRUE) {
            BaseType_t taken = xSemaphoreTake(m->handle, 0U);
            if (taken == pdTRUE) {
                for (uint32_t i = 0U; i < 4U; ++i) {
                    if (waiters[i].task == self) {
                        waiters[i].task = NULL;
                        (void)xSemaphoreTake(waiters[i].signal, 0U);
                        break;
                    }
                }
                m->saved_prio = uxTaskPriorityGet(NULL);
                owners[target] = self;
                if (m->saved_prio < m->ceiling_prio) {
                    vTaskPrioritySet(NULL, m->ceiling_prio);
                }
                taskEXIT_CRITICAL();
                return;
            }
        }
        uint32_t slot = waiter_index(self);
        taskEXIT_CRITICAL();
        BaseType_t woke = xSemaphoreTake(waiters[slot].signal, portMAX_DELAY);
        configASSERT(woke == pdTRUE);
    }
}

void pc_mutex_unlock(pc_mutex_t *m)
{
    configASSERT(m != NULL);
    configASSERT(m->handle != NULL);
    uint32_t target = mutex_index(m);
    TaskHandle_t self = xTaskGetCurrentTaskHandle();

    taskENTER_CRITICAL();
    configASSERT(owners[target] == self);
    vTaskPrioritySet(NULL, m->saved_prio);
    BaseType_t given = xSemaphoreGive(m->handle);
    configASSERT(given == pdTRUE);
    owners[target] = NULL;
    for (uint32_t i = 0U; i < 4U; ++i) {
        if (waiters[i].task != NULL) {
            (void)xSemaphoreGive(waiters[i].signal);
        }
    }
    taskEXIT_CRITICAL();
}
