#include "heap.h"
#include "mpu.h"
#include "stack_pool.h"

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((uint32_t)(a)-1))
#define HEAP_SIZE (uint8_t *)&_eheap - (uint8_t *)&_sheap

extern uint8_t _sheap;
extern uint8_t _eheap;
static heap_block_t *heap_head = 0;

void os_heap_init() {
    heap_head = (heap_block_t *)&_sheap;
    heap_head->is_free = 1;
    heap_head->size = HEAP_SIZE - sizeof(heap_block_t);
    heap_head->next = 0;
    os_stack_pool_init();
}

#ifdef FIRST_FIT
void *os_malloc(uint32_t size) {
    heap_block_t *current = heap_head;
    uint32_t aligned_size;
    if(mpu_status == 0) {
        aligned_size = ALIGN(size, 8);
    }
    else {
        aligned_size = ALIGN(size, 32);
    }
    while(current != 0) {
        if(current->is_free == 1 && current->size >= aligned_size) {
            if(current->size > aligned_size + sizeof(heap_block_t)) {
                heap_block_t *new = current;
                new = (heap_block_t *)((uint8_t *)current + sizeof(heap_block_t) + aligned_size);
                new->is_free = 1;
                new->size = current->size - aligned_size - sizeof(heap_block_t);
                new->next = current->next;
                current->is_free = 0;
                current->size = aligned_size;
                current->next = new;
            }
            else {
                current->is_free = 0;
            }
            return (void *)((uint8_t *)current + sizeof(heap_block_t));
        }
        current = current->next;
    }
    return 0;
}
#elif defined BEST_FIT
void *os_malloc(uint32_t size) {
    heap_block_t *current = heap_head;
    uint32_t aligned_size;
    if(mpu_status == 0) {
        aligned_size = ALIGN(size, 8);
    }
    else {
        aligned_size = ALIGN(size, 32);
    }
    heap_block_t *temporary = current;
    uint32_t min_size = HEAP_SIZE;
    uint8_t is_available = 0;
    while(temporary != 0) {
        if(temporary->is_free == 1 && temporary->size >= aligned_size) {
            if(temporary->size < min_size) {
                current = temporary;
                min_size = temporary->size;
                is_available = 1;
            }
        }
        temporary = temporary->next;
    }
    if(is_available == 1) {
        if(current->size > aligned_size + sizeof(heap_block_t)) {
            heap_block_t *new = current;
            new = (heap_block_t *)((uint8_t *)current + sizeof(heap_block_t) + aligned_size);
            new->is_free = 1;
            new->size = current->size - aligned_size - sizeof(heap_block_t);
            new->next = current->next;
            current->is_free = 0;
            current->size = aligned_size;
            current->next = new;
        }
        else {
            current->is_free = 0;
        }
        return (void *)((uint8_t *)current + sizeof(heap_block_t));
    }
    else {
        return (void *)0;
    }
}
#endif

void os_mfree(void *mem) {
    if(mem == 0) {
        return;
    }
    heap_block_t *block = (heap_block_t *)((uint8_t *)mem - sizeof(heap_block_t));
    block->is_free = 1;
    heap_block_t *current = heap_head;
    while(current->next != 0) {
        if(current->is_free == 1 && current->next->is_free == 1) {
            current->size = current->size + sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
        }
        else {
            current = current->next;
        }
    }
}
