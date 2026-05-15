# Task Control Block

## The problem

A bare-metal CPU only ever does one thing at a time, it fetches an instruction, executes it, moves to the next. There is no concept of "tasks" there is just the program counter that keeps moving forward

So when you want to run multiple tasks concurrently, you need to create a system that fakes it, The trick is to run Task A for a short period, after the period is ends, freeze its entire state, then throw Task B's state and run it for another small period, and repeat fast enough that both feel like they are running simultaneously. This is called **context switching**.

now the question is : where did the state of the task running go? when its period ends

Every task's live state lives in the CPU registers, the program counter (`PC`), the stack pointer (`SP`), and all the general purpose registers (`r0–r11`, `LR`). When you switch away from a task, all of that needs to be saved somewhere safe so it can be restored exactly as it was. That somewhere is the **Task Control Block**.

---

## The TCB struct

```c
typedef struct {
    uint32_t *stack_ptr;
    int task_num;
    os_task_state state;
    uint32_t delay_ticks;
    void *waiting_for_resource;
} os_tcb_t;
```

The most important field is `stack_ptr`. This is the only thing the TCB directly stores, the stack pointer of the task when it is not running is saved, everything else (local variables, return addresses, register values) lives on the task's stack pointed by this pointer. it can be imagined as the bookmark

The `task_num` is a unique identifier for each task

The `state` field tells the scheduler whether the task is eligible to run:

```c
typedef enum {
    TASK_READY,           // can be scheduled
    TASK_BLOCKED,         // sleeping via os_delay()
    TASK_MUTEX_WAITING,   // blocked on a lock
} os_task_state;
```

`delay_ticks` counts down to zero every millisecond in the SysTick handler. When it hits zero, the task flips back to `TASK_READY`. `waiting_for_resource` is a pointer to whichever mutex the task is parked on, so `os_mutex_give()` knows who to wake up. (the `void *` is used as the type of resource is unknown)

---

## Static allocation: 5 tasks, for now

All memory for TCBs and stacks is allocated at compile time as global arrays:

```c
#define OS_MAX_TASKS_NUM   5
#define OS_TASK_STACK_SIZE 128

os_tcb_t os_tasks[OS_MAX_TASKS_NUM];
uint32_t os_task_stacks[OS_MAX_TASKS_NUM][OS_TASK_STACK_SIZE];
```

This means the linker lays out a fixed block of SRAM at compile time, 5 TCB structs and 5 stacks of 128 words each, totaling `(5 × 128 × 4) = 2560 bytes` of stack space. No heap, no `malloc`, nothing dynamic.[as of now]

The tradeoff is deliberate. Static allocation is predictable you know exactly how much SRAM the kernel uses before you flash the chip, there are no fragmentation surprises, and there is nothing that can fail at runtime due to memory pressure. For an initial build it is the right call. The plan is to move to dynamic TCB allocation with per-task configurable stack sizes as the kernel matures.

---

## Journey of a task, from boot to first instruction

### 1. Boot : the startup file runs

Before `main()` is called, the startup assembly in `startup_stm32f429zi.s` runs. It copies the `.data` section from flash into SRAM and zeroes the `.bss` section (where all the global arrays live):

```asm
Reset_Handler:
    /* Copy .data from flash to SRAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata

copy_data:
    cmp r0, r1
    bge done_data
    ldr r3, [r2], #4
    str r3, [r0], #4
    b copy_data

done_data:
    /* Step 2: Zero out .bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0

zero_bss:
    ...
    ...

done_bss:
    bl main
```

After this `os_tasks[]` and `os_task_stacks[][]` exist in SRAM, all zeroed. No tasks have been created yet.

### 2. `os_task_create()` : building the fake stack frame

When `main()` calls `os_task_create(task_function)`, the kernel carves out one slot from the static arrays and builds a fake exception frame at the top of that task's stack:

```c
uint32_t *task_stack_ptr = &task_stack[OS_TASK_STACK_SIZE - 1]; // top of stack

*task_stack_ptr = 0x01000000;           // xPSR  — Thumb mode bit set
task_stack_ptr -= 1;
*task_stack_ptr = (uint32_t)task_function; // PC — where to jump on first run
task_stack_ptr -= 1;
*task_stack_ptr = 0xFFFFFFFD;           // LR  — EXC_RETURN: return to Thread, use PSP
task_stack_ptr -= 13;                   // r0–r3, r12 (auto-saved), then r4–r11 (manual)

task->stack_ptr = task_stack_ptr;       // bookmark saved in TCB
task->state     = TASK_READY;
```

This is the key insight: the stack is pre-loaded to look exactly like the task was already running and got interrupted. When the CPU later "returns from the exception" into this task for the first time, it pops `xPSR`, `PC`, and `LR` off the stack and jumps straight into `task_function`, no special first-run handling needed.

The layout on the stack looks like this:

```
High address (top of stack)
┌───────────────┐ ← &task_stack[127]
│  xPSR         │  0x01000000
├───────────────┤
│  PC           │  address of task_function
├───────────────┤
│  LR           │  0xFFFFFFFD
├───────────────┤
│  r12          │  0x00000000
├───────────────┤
│  r3           │  0x00000000
├───────────────┤
│  r2           │  0x00000000
├───────────────┤
│  r1           │  0x00000000
├───────────────┤
│  r0           │  0x00000000
├───────────────┤
│  r11          │  0x00000000
│  r10          │   ...
│  r9           │
│  r8           │
│  r7           │
│  r6           │
│  r5           │
│  r4           │
├───────────────┤ ← task->stack_ptr saved here
│  (free space) │
└───────────────┘
Low address
```

The top 8 words (`xPSR` -> `r0`) are the hardware-saved frame, the Cortex-M exception entry pushes and pops these automatically. The next 8 (`r4–r11`) are software-saved because the hardware does not touch them, PendSV handles those manually.

### 3. `os_start()` : handing control to the first task via SVC

After all user tasks are created, `main()` calls `os_start()`:

```c
void os_start(void) {
    os_task_create(os_idle_task); // always register the idle task last
    SHPR3 = 255;                  // set PendSV to lowest priority
    __asm volatile ("svc 0");     // trigger Supervisor Call
}
```

The idle task is a safety backup, when every other task is blocked, the scheduler falls back to it and the CPU executes `wfi` (wait for interrupt) to sleep until the next SysTick fires, rather than spinning in a dead loop burning power.

`SHPR3 = 255` sets PendSV to the lowest possible priority, this is important because it guarantees the context switch only runs *after* all higher-priority interrupts (SysTick, peripherals) have finished. You never want a context switch to interrupt a half-finished interrupt handler.

Then `svc 0` fires the **SVC (Supervisor Call) exception**, which vectors to `SVC_Handler` in the startup assembly.

### 4. `SVC_Handler` : launching the first task

```asm
SVC_Handler:
    ldr r3, =os_current_task_ptr   // load address of the pointer variable
    ldr r1, [r3]                   // r1 = pointer to current TCB
    ldr r0, [r1]                   // r0 = task->stack_ptr (top of fake frame)
    ldmia r0!, {r4-r11}            // pop r4–r11 off the stack into registers
    msr psp, r0                    // set PSP to point at remaining hw frame
    mov r0, #0
    msr basepri, r0                // unmask all interrupts
    ldr r14, =0xFFFFFFFD           // EXC_RETURN: return to Thread mode, PSP
    bx r14                         // "return from exception" CPU pops hw frame
```

This is a one-time trick. The CPU is currently in Handler mode (inside the SVC exception). By loading `0xFFFFFFFD` into `LR` and executing `bx lr`, the CPU thinks it is returning from an exception. It pops `r0–r3`, `r12`, `LR`, `PC`, and `xPSR` off the PSP stack, which is exactly the fake frame we built, and jumps to `task_function`. The OS is now running.

---

## Keeping it running : SysTick and PendSV

Every millisecond, SysTick fires:

```c
void SysTick_Handler(void) {
    system_ticks++;
    os_decrement_blocked_tasks();  // count down delay_ticks for BLOCKED tasks
    ICSR |= (1 << 28);             // pend PendSV
}
```

It does three things: increments the global clock, unblocks any tasks whose delay has expired, and sets the PendSV pending bit. Since PendSV is the lowest-priority exception, it only runs once all other handlers have returned

### 5. `PendSV_Handler` : the actual context switch

```asm
PendSV_Handler:
    mrs r0, psp                    // r0 = current PSP (top of current task's hw frame)
    stmdb r0!, {r4-r11}            // push r4–r11 onto current task's stack
    ldr r1, =os_current_task_ptr
    ldr r2, [r1]
    str r0, [r2]                   // save updated SP into current TCB->stack_ptr

    push {r14}
    bl os_schedule_next_task       // pick the next READY task, update os_current_task_ptr
    pop {r14}

    ldr r1, =os_current_task_ptr
    ldr r2, [r1]
    ldr r0, [r2]                   // load next task's saved stack_ptr
    ldmia r0!, {r4-r11}            // restore r4–r11 from next task's stack
    msr psp, r0                    // point PSP at next task's hw frame
    bx r14                         // EXC_RETURN — CPU pops hw frame, jumps to next task
```

This is the heart of the RTOS. The save half pushes `r4–r11` onto the current task's stack and stores the new SP into the TCB. The restore half loads the next task's SP from its TCB, pops its `r4–r11`, sets PSP, and returns, at which point the CPU pops the hardware frame and resumes execution exactly where that task left off.

---

## The full flow

```
boot
 │
 ▼
Reset_Handler          zeros SRAM, copies .data, calls main()
 │
 ▼
os_task_create() ×N    builds fake stack frame for each task, fills TCB
 │
 ▼
os_start()             creates idle task, sets PendSV priority, fires SVC
 │
 ▼
SVC_Handler            loads TCB[0]'s fake frame, returns into first task
 │
 ▼
task runs...
 │
 ▼ (every 1ms)
SysTick_Handler        tick++, unblock expired tasks, pend PendSV
 │
 ▼
PendSV_Handler         save current task's registers into its stack + TCB
                       call os_schedule_next_task()
                       load next task's registers from its stack + TCB
                       return into next task
 │
 ▼
next task runs...      (repeats forever)
```

The TCB is what makes this whole loop possible. Every time PendSV runs, it uses `os_current_task_ptr->stack_ptr` as the address to park or restore a task's state. Without the TCB there is nowhere to store that pointer between switches, and the illusion of concurrent tasks falls apart.