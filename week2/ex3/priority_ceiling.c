#include<priority_ceiling.h>
#include<stddef.h>
#include<FreeRTOS.h>
#include<task.h>


void pc_mutex_create_static(pc_mutex_t *m, UBaseType_t ceiling_prio){

    configASSERT(m != NULL);
    configASSERT(ceiling_prio < configMAX_PRIORITIES);

    m->ceiling_prio = ceiling_prio;
    m->saved_prio = 0;
    m->handle = xSemaphoreCreateMutexStatic(&m->storage);
    configASSERT(m->handle != NULL);
}

void pc_mutex_lock(pc_mutex_t *m){
    configASSERT(m != NULL);
    configASSERT(m->handle != NULL);
    configASSERT(xSemaphoreTake(m->handle, portMAX_DELAY) == pdTRUE);

    m->saved_prio = uxTaskPriorityGet(NULL);
    if (m->saved_prio < m->ceiling_prio) {
        vTaskPrioritySet(NULL, m->ceiling_prio);
    }
}

void pc_mutex_unlock(pc_mutex_t *m){
    configASSERT(m != NULL);
    configASSERT(m->handle != NULL);
    vTaskPrioritySet(NULL, m->saved_prio);
    configASSERT(xSemaphoreGive(m->handle) == pdTRUE);
}