# Mutex

## The problem

The RTOS is now switching between tasks every millisecond, but it creates a new class of bug.

Say Task A is sending a string over UART. It has written three bytes and the SysTick fires mid-string. PendSV kicks in, saves Task A, and switches to Task B. Task B also wants to print something, so it starts writing its own bytes to UART. Now both strings are interleaved on the wire and the output is garbage.

This is a **race condition** two tasks stepping on the same shared resource with no coordination between them. It is not a bug you can catch by reading the code of either task in isolation. It only exists because the scheduler can interrupt either task at any point.

The fix is **mutual exclusion** a mechanism that lets one task claim exclusive ownership of a resource, do its work, and release it, while all other tasks that want the same resource are made to wait. That mechanism is a **mutex**.

---

## The struct

```c
typedef struct {
    int is_locked;
    int task_num_owner;
    int recursive_depth;
} os_mutex_t;
```

Three integers. `is_locked` is the flag `0` means free, `1` means taken. `task_num_owner` records which task is currently holding it. `recursive_depth` counts how many times the owning task has called `os_mutex_take()` without a matching `os_mutex_give()`.

That third field is what makes this a **recursive mutex**, explained in detail below.

A mutex is declared as a global and initialised once before any task runs:

```c
os_mutex_t uart_mutex;

int main(void) {
    os_mutex_init(&uart_mutex);
    os_task_create(task_a);
    os_task_create(task_b);
    os_start();
}
```

`os_mutex_init()` zeroes all three fields:

```c
void os_mutex_init(os_mutex_t *mutex) {
    mutex->is_locked = 0;
    mutex->task_num_owner = -1;
    mutex->recursive_depth = 0;
}
```

`-1` for `task_num_owner` means that "nobody owns this".

---

## Why recursive mutexes?

Before getting into the code, it is worth understanding the problem that `recursive_depth` solves.

Imagine a task that calls a helper function, and both the task and the helper want to protect the same resource:

```c
void helper(void) {
    os_mutex_take(&uart_mutex);   // inner take
    uart_send_string("helper\r\n");
    os_mutex_give(&uart_mutex);
}

void task_a(void) {
    while(1) {
        os_mutex_take(&uart_mutex);   // outer take
        uart_send_string("task_a\r\n");
        helper();                     // calls take again on same mutex!
        os_mutex_give(&uart_mutex);
    }
}
```

Without recursive support, when `helper()` calls `os_mutex_take()` the mutex is already locked and `task_num_owner` is already `task_a`. The old code would see `is_locked == 1`, park the task as `TASK_MUTEX_WAITING`, and wait for someone to call `os_mutex_give()`. But `task_a` is the only one that can give it, and it is now parked waiting for itself. **Deadlock.**

The fix is simple in concept: if the task calling `os_mutex_take()` is already the owner, let it in and just increment a counter. Only when that counter reaches zero on the give side does the mutex actually unlock.

---

## flow of a mutex

### Scenario

Two tasks share a mutex. Task A also calls a helper that re-takes the same mutex internally.

```
Task A runs                    Task B runs
────────────────────────────────────────────────
os_mutex_take()
  → free, grab it
  → is_locked=1, depth=1
  [working...]
  helper() → os_mutex_take()
    → locked, but I AM the owner
    → depth=2, pass through
    [helper working...]
  os_mutex_give()  ← from helper
    → depth-- = 1, still > 0
    → do NOT unlock yet, return
  [still working...]
os_mutex_give()  ← from task_a
  → depth-- = 0
  → actually unlock now
  → wake Task B → TASK_READY
                             → scheduler picks Task B
                             os_mutex_take()
                               → free now, grab it
                               → is_locked=1, depth=1
                               [working...]
                             os_mutex_give()
                               → depth-- = 0, unlock
```

That is the full story. Now look at the code that makes each step happen.

---

## `os_mutex_take()` — claiming the lock

```c
void os_mutex_take(os_mutex_t *mutex) {
    while(1) {
        __asm volatile ("cpsid i");              // 1. disable interrupts

        if (mutex->is_locked == 0) {
            mutex->is_locked = 1;                // 2. free — claim it
            mutex->task_num_owner = os_current_task_ptr->task_num;
            mutex->recursive_depth = 1;
            __asm volatile ("cpsie i");
            break;
        }
        else if (mutex->is_locked == 1 &&
                 mutex->task_num_owner == os_current_task_ptr->task_num) {
            mutex->recursive_depth++;            // 3. already mine — go deeper
            __asm volatile ("cpsie i");
            break;
        }
        else {
            os_current_task_ptr->state = TASK_MUTEX_WAITING;  // 4. someone else owns it — park
            os_current_task_ptr->waiting_for_resource = (void *)mutex;
            __asm volatile ("cpsie i");
            ICSR |= (1 << 28);                   // yield — trigger PendSV now
        }
    }
}
```

The loop now has three branches instead of two:

**Branch 1 -> mutex is free.** Claim it, set owner, set `recursive_depth = 1`. The depth starts at 1, not 0, because the task is now inside the mutex once.

**Branch 2 -> mutex is taken, but by me.** This is the recursive case. The same task is calling take again. Rather than deadlocking, just increment `recursive_depth` and break out. The task passes through as if it got the lock — because it already has it.

**Branch 3 -> mutex is taken by someone else.** Park as `TASK_MUTEX_WAITING`, store the mutex pointer in `waiting_for_resource`, then immediately yield via PendSV. No point burning the timeslice.

### Why the `while(1)` loop?

A task woken by `os_mutex_give()` is not handed the mutex directly, it is just flipped to `TASK_READY`. When the scheduler picks it up it loops back into `os_mutex_take()` and tries branch 1 again. If another task grabbed it in between it falls to branch 3 and parks itself again. The loop handles all of that naturally.

### Why `cpsid i` / `cpsie i`?

The check-and-set sequence across all three branches is multiple memory accesses. Without disabling interrupts, SysTick can fire between the read and the write, the scheduler can switch to another task mid-check, and two tasks can both think they own the mutex. `cpsid i` makes the entire check-and-set atomic. Interrupts are re-enabled before triggering PendSV in branch 3 because you need them enabled for the context switch to actually execute.

---

## `os_mutex_give()` : releasing the lock

```c
void os_mutex_give(os_mutex_t *mutex) {
    __asm volatile ("cpsid i");

    if (mutex->task_num_owner == os_current_task_ptr->task_num) {  // 1. verify ownership
        mutex->recursive_depth--;                                   // 2. unwind one level

        if (mutex->recursive_depth > 0) {
            __asm volatile ("cpsie i");
            return;                                                 // 3. still nested, hold on
        }

        mutex->is_locked = 0;                                       // 4. fully unwound, release
        mutex->task_num_owner = -1;

        for (uint32_t i = 0; i < os_get_task_count(); i++) {       // 5. scan for waiters
            if (os_tasks[i].state == TASK_MUTEX_WAITING) {
                if (os_tasks[i].waiting_for_resource == (void *)mutex) {
                    os_tasks[i].state = TASK_READY;                 // 6. wake first waiter
                    os_tasks[i].waiting_for_resource = 0;
                    break;
                }
            }
        }
    }

    __asm volatile ("cpsie i");
}
```

### Ownership check

Only the task that took the mutex can give it back. If `task_num_owner` does not match the current task the function returns silently, the mutex is left exactly as it was.

### Depth decrement

```c

mutex->recursive_depth--;
if (mutex->recursive_depth > 0) {
    __asm volatile ("cpsie i");
    return;
}
```

Every `os_mutex_give()` call unwinds one level. If the depth is still above zero after decrementing, the task is still nested inside at least one other take, so the mutex stays locked and the function returns early. The actual unlock only happens when depth reaches zero, meaning every take has been matched by a give.

This is the pairing contract: **one `os_mutex_give()` for every `os_mutex_take()`**

### Scanning for waiters

Once depth hits zero the mutex is truly free. The code walks `os_tasks[]` and wakes the first task in `TASK_MUTEX_WAITING` that is waiting on this specific mutex:

```c
if (os_tasks[i].waiting_for_resource == (void *)mutex)
```

That task is flipped to `TASK_READY` and its `waiting_for_resource` is cleared. On the next scheduler run it loops back into `os_mutex_take()`, hits branch 1, and claims the lock.

---

## Current limitations

**First-come-first-served, not priority-ordered.** `os_mutex_give()` wakes the first waiter it finds scanning `os_tasks[]` from index 0. If Task 4 has been waiting longer than Task 1, Task 1 still gets woken first because it has a lower index. A production kernel would maintain a proper wait queue ordered by priority or arrival time.
