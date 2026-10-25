#include<spsc_ring.h>
#include<stm32f1xx.h>
#include<stddef.h>

#define RING_MASK (W2E2_RING_CAPACITY - 1U)

void spsc_ring_init(spsc_ring_t *ring){
    if (ring == NULL){
        return;
    }
    ring->head = 0U;
    ring->tail = 0U;

}

bool spsc_ring_push_from_isr(spsc_ring_t *ring, spsc_elem_t elem){
    if (ring == NULL){
        return false;
    }
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    
    if ((uint32_t)(head - tail) >= W2E2_RING_CAPACITY ){
        return false;

    }

    ring->storage[head & RING_MASK] = elem;

    __DMB();

    ring->head = head + 1U;
    return true;
}

bool spsc_ring_pop(spsc_ring_t *ring, spsc_elem_t *out){
    if (ring == NULL || out == NULL){
        return false;
    }
    uint32_t tail = ring->tail;
    uint32_t head = ring->head;
 
    if (head == tail){
        return false;
    }
    __DMB();

    *out = ring->storage[tail & RING_MASK];

    __DMB();
    
    ring->tail = tail + 1U;
    return true;

}

uint32_t spsc_ring_count(const spsc_ring_t *ring){
    if (ring == NULL){
        return 0;
    }
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;

    return head - tail;
}

