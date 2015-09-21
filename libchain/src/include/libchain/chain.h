#ifndef CHAIN_H
#define CHAIN_H

typedef void (task_func_t)(void);

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 */
static inline void transition_to(task_func_t *next_task)
{
    // TODO: re-use the top-of-stack address used in entry point
    //       probably need to write a custom entry point in asm, and
    //       use it instead of the C runtime one.
    __asm__ (
        "mov #0x2400, r1\n"
        "br %0\n"
        : : "r" (next_task)
    );
}

#endif // CHAIN_H
