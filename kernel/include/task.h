#ifndef H_TASK
#define H_TASK

#include <stdint.h>

#define OS_MAX_TASKS_NUM 5
#define OS_TASK_STACK_SIZE 128

typedef struct {
    uint32_t *stack_ptr;
    int task_num;
} os_tcb_t;

extern os_tcb_t *os_current_task_ptr;

int os_task_create(void (*task_fucntion)(void));

#endif