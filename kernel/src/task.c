#include "task.h"

os_tcb_t os_tasks[OS_MAX_TASKS_NUM];
uint32_t os_task_stacks[OS_MAX_TASKS_NUM][OS_TASK_STACK_SIZE];
os_tcb_t *os_current_task_ptr = 0;

static uint32_t os_task_count = 0;

int os_task_create(void (*task_function)(void)){
    if(os_task_count >= OS_MAX_TASKS_NUM){
        return -1;
    }
    os_tcb_t *task = &os_tasks[os_task_count];
    uint32_t *task_stack = os_task_stacks[os_task_count];
    uint32_t *task_stack_ptr = &task_stack[OS_TASK_STACK_SIZE - 1];

    *task_stack_ptr = 0x01000000;
    task_stack_ptr -= 1;
    *task_stack_ptr = (uint32_t)task_function;
    task_stack_ptr -= 1;
    *task_stack_ptr = 0xFFFFFFFD;
    task_stack_ptr -= 13;

    task->stack_ptr = task_stack_ptr;
    task->task_num = os_task_count;

    if(os_task_count == 0){
        os_current_task_ptr = task;
    }
    os_task_count += 1;
    return 0;
}
