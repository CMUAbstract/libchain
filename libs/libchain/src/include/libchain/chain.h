#ifndef CHAIN_H
#define CHAIN_H

typedef void (task_func_t)(void);

extern task_func_t * volatile curtask;

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 */
static inline void transition_to(task_func_t *next_task)
{
    // reset stack pointer
    // update current task pointer
    // jump to next task

    // NOTE: the order of these does not seem to matter, a reboot
    // at any point in this sequence seems to be harmless.
    //
    // NOTE: It might be a bit cleaner to reset the stack and
    // set the current task pointer in the function *prologue* --
    // would need compiler support for this.

    // TODO: re-use the top-of-stack address used in entry point
    //       probably need to write a custom entry point in asm, and
    //       use it instead of the C runtime one.
    __asm__ (
        "mov #0x2400, r1\n"
        "mov %0, %1\n"
        "br %0\n"
        : : "r" (next_task),
            "r" (curtask)
    );
}

#endif // CHAIN_H
